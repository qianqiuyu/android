/*参考usbkbd.c*/
/*http://blog.chinaunix.net/uid-12461657-id-2975401.html*/
/*http://blog.csdn.net/opencpu/article/details/6876083 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

static char *name = "Linux Device Driver";
module_param(name, charp, S_IRUGO);

static const unsigned char usb_kbd_keycode[256] = {/*键值码表*/
	  0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
	 50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
	  4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
	 27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
	 65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
	105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
	 72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
	191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
	115,114,  0,  0,  0,121,  0, 89, 93,124, 92, 94, 95,  0,  0,  0,
	122,123, 90, 91, 85,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
	150,158,159,128,136,177,178,176,142,152,173,140
};
static int len;
static struct input_dev *uk_dev;
static char *usb_buf;
static dma_addr_t usb_buf_phys;
static struct urb *uk_urb;

static struct usb_device_id keyboard_id_table [] = {
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_KEYBOARD) },/*3,1,1分别表示接口类,接口子类,接口协议;3,1,1为键盘接口类;鼠标为3,1,2*/
	{ }	/* Terminating entry */
};

void buffer_copy (char *usb_buf_source , char *usb_buf_target , int len)
{
	int i;
	for(i=0;i<len;i++)
	{
		usb_buf_target[i] = usb_buf_source[i];
	}
}

static void keyboard_irq(struct urb *urb)
{
	struct usb_kbd *kbd = urb->context;
	int i;
	static char usb_buf_pre_val[8];
#if 0
	static int cnt = 0;
	printk("data cnt %d: ", ++cnt);
	for (i = 0; i < len; i++)
	{
		printk("%02x ", usb_buf[i]);
	}
	printk("\n");
#endif

//	for (i = 0; i < 8; i++)/*检测修饰按键，8次的值依次是:29-42-56-125-97-54-100-126*/
//		input_report_key(uk_dev, usb_kbd_keycode[i + 224], (usb_buf[0] >> i) & 1);

	for (i = 2; i < 8; i++) {/*若同时只按下1个按键则在第[2]个字节,若同时有两个按键则第二个在第[3]字节，类推最多可有6个按键同时按下*/ 
		if(usb_buf[i]!=usb_buf_pre_val[i])
		{
			if((usb_buf[i] ) ? 1 : 0)
				input_report_key(uk_dev, usb_kbd_keycode[usb_buf[i]],  1);
			else
				input_report_key(uk_dev, usb_kbd_keycode[usb_buf_pre_val[i]],  0);
		}
	}
	
/*同步设备,告知事件的接收者驱动已经发出了一个完整的报告*/ 
	input_sync(uk_dev);

	buffer_copy(usb_buf , usb_buf_pre_val , 8);/*防止未松开时被当成新的按键处理*/
	
	/* 重新提交urb */
	usb_submit_urb(uk_urb, GFP_KERNEL);
}

static int keyboard_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor *endpoint;
	int pipe;

	/*当前选择的interface*/
	interface = intf->cur_altsetting;
	/*获取端点描述符*/
	endpoint = &interface->endpoint[0].desc;

	/* a. 分配一个input_dev */
	uk_dev = input_allocate_device();    
	if ( !uk_dev)
		printk("allocate false \n");
	
	/* b. 设置 */
	/* b.1 能产生哪类事件 */
	set_bit(EV_KEY, uk_dev->evbit);/*键码事件*/
	set_bit(EV_REP, uk_dev->evbit);/*自动重覆数值*/;
	uk_dev->ledbit[0] = BIT(LED_NUML)/*数字灯*/ | BIT(LED_CAPSL)/*大小写灯*/ | BIT(LED_SCROLLL)/*滚动灯*/ | BIT(LED_COMPOSE) | BIT(LED_KANA);
	
	
	/* b.2 能产生哪些事件 */
	for (len = 0; len < 255; len++) 
		set_bit(usb_kbd_keycode[len], uk_dev->keybit);
	clear_bit(0, uk_dev->keybit);
	
	/* c. 注册 */
	input_register_device(uk_dev);
	
	/* d. 硬件相关操作 */
	/* 数据传输3要素: 源,目的,长度 */
	/* 源: USB设备的某个端点 */
	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);/*将endpoint设置为中断IN端点*/

	/* 长度: */
	len = endpoint->wMaxPacketSize;

	/* 目的: */
	//usb_buf = usb_buffer_alloc(dev, len, GFP_ATOMIC, &usb_buf_phys);
	usb_buf = usb_alloc_coherent(dev, len, GFP_ATOMIC, &usb_buf_phys);

	/* 使用"3要素" */
	/* 分配usb request block */
	uk_urb = usb_alloc_urb(0, GFP_KERNEL);
	/* 使用"3要素设置urb" */
	usb_fill_int_urb(uk_urb, dev, pipe, usb_buf, len, keyboard_irq, NULL, endpoint->bInterval);
	uk_urb->transfer_dma = usb_buf_phys;
	uk_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/* 使用URB */
	usb_submit_urb(uk_urb, GFP_KERNEL);/*发送USB请求块*/
	
	//usb_set_intfdata(iface, kbd);/*设置接口私有数据*/
	/*kbd用来存放一些数据，如厂商名字等等，这没用到*/

	//uk_dev->event = my_kbd_event;/* 注册事件处理函数入口*/ 

	//uk_dev->open = my_kbd_open;/*注册设备打开函数入口*/ 

	//uk_dev->close = my_kbd_close;/*注册设备关闭函数入口*/
	/*打印设备信息*/
	//printk("VID = 0x%x, PID = 0x%x\n", dev->descriptor.idVendor, dev->descriptor.idProduct);
	//printk("USB VERS = 0x%x, PID = 0x%x\n", dev->descriptor.bcdUSB);

	/*insmod my_keyboard.ko name='KITE'*/
	printk(KERN_INFO "test name:%s\n", name);
	return 0;
}

static void keyboard_disconnect(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev(intf);

	//usb_set_intfdata(intf, NULL);/*设置接口的私有数据为NULL*/ 
	//printk("disconnect usbmouse!\n");
	usb_kill_urb(uk_urb);
	usb_free_urb(uk_urb);

	usb_free_coherent(dev, len, usb_buf, usb_buf_phys);
	input_unregister_device(uk_dev);
	input_free_device(uk_dev);
}

/* 1. 分配/设置usb_driver */
static struct usb_driver keyboard_driver = {/*任何一个LINUX下的驱动都有个类似的驱动结构*/
	.name		= "my_keyboard",
	.probe		= keyboard_probe,/*驱动探测函数,加载时用到*/
	.disconnect	= keyboard_disconnect,
	.id_table	= keyboard_id_table,/*驱动设备ID表,用来指定设备或接口*/ 
};

static int keyboard_init(void)
{
	/* 2. 注册 */
	int result = usb_register(&keyboard_driver);
	if (result != 0) /*注册失败*/ 
		printk("register false \n");
	return 0;
}

static void keyboard_exit(void)
{
	usb_deregister(&keyboard_driver);	
}

module_init(keyboard_init);
module_exit(keyboard_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kite");/*modinfo my_keyboard.ko*/
MODULE_DESCRIPTION("A usb keyboard Module for testing module ");
MODULE_VERSION("V1.0");