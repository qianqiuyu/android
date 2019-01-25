#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/poll.h>
#include <linux/cdev.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <mach/gpio-samsung.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/gpio.h>
#include <linux/clk.h>

/* 1. 确定主设备号 */
static int major;
static struct cdev pwm_cdev;
static struct class *cls;

static volatile unsigned long *gpbcon;
static volatile unsigned long *gpbdat;
static volatile unsigned long *tcfg0;
static volatile unsigned long *tcfg1;
static volatile unsigned long *tcon;
static volatile unsigned long *tcntb0;//计数周期
static volatile unsigned long *tcmpb0;//占空比
/*由于s3c2440的定时器具有双缓存，
 *因此可以在定时器运行的状态下，改变这两个寄存器的值，
 *它会在下个周期开始有效。*/
#define TCFG0_PRESCALER_MASK		0xff
#define TCFG1_MUX_MASK			0xf

static int pwm_open(struct inode *inode, struct file *file)
{	
	unsigned long tcnt;
	struct clk *clk_p;
	unsigned long plck;

	// MINOR(inode->i_rdev); //提取次设备号
	// MAJOR(inode->i_rdev); //提取主设备号
	
	//s3c_gpio_cfgpin(S3C2410_GPB(0), S3C_GPIO_SFN(2) );/*定时器复用功能*/
       //gpio_set_value(S3C2410_GPB(0), 0);
       *gpbcon &= ~(0x3<<(0*2));
	*gpbcon |= (0x2<<(0*2));					/*复用模式*/

	/*定时器输入时钟频率 = PCLK / {预分频值+1} / {分频值}*/
	*tcfg0 &= ~TCFG0_PRESCALER_MASK;		/*TCFG0_PRESCALER_MASK=0xff*/
	*tcfg0 |= (50-1);							/*预分频为50HZ*/ 

	*tcfg1 &= ~TCFG1_MUX_MASK;				/*清零，TCFG1_MUX_MASK=0x0f*/
	*tcfg1 |= (3<<0);							/*pwm 定时器0 进行16 分频*/

	/*clk_p = clk_get(NULL, "timers");
	if(IS_ERR(clk_p))
	{
		printk("fail to get plck clock source \n");
		return 0 ;
	}
	plck = clk_get_rate(clk_p);
	printk("plck clock is  %d\n",plck);*/
    	//频率 = (50MHz/50/16)=62500;			/*得到定时器的输入时钟，进而设置PWM的调制频率*/ 
	
	tcnt=62500;								/*62500/62500=1Hz*/
	*tcntb0 = tcnt ;							/*定时器 0 的计数缓冲器的值*/
	*tcmpb0 = tcnt>>1 ;						/*定时器 0 的比较缓冲器的值*//*50%占空比*/
	
	*tcon &= ~0x000f;						/*清零[3:0] 定时器0*/
	/*tcon[3:0]
	*[3] =	自动重载开启或关闭 :0 = 单稳态 1 = 间隙模式（自动重载） 
	*[2] =	输出变相开启或关闭 :0 = 关闭变相 1 = TOUT0 变换极性
	*[1] =	手动更新 :0 = 无操作 1 = 更新 TCNTB0和 TCMPB0 
	*[0] =	启动或停止 :0 = 停止定时器 0 1 = 启动定时器 0 
	*手动更新:开始定时时，一定要把这位清零，否则是不能开启定时器的*/
	*tcon |= (1<<1);							/*手动装载*/
	*tcon &= ~(1<<1);						/*清除手动装载*/
	*tcon |= 0x0d;							/*0b 1101=自动重载、变化极性、不手动更新、启动定时器*/

	
	/*pin_val  = __raw_readl(S3C2410_GPBDAT);	 //读取寄存器
	pin_val&= ~1;     // 输出低电平 
	__raw_writel(pin_val, S3C2410_GPBDAT);*/
       	
	return 0;
}

static int pwm_read(struct file *filp, char __user *buff, size_t count , loff_t *offp)
{
	unsigned int pinval;
	pinval = gpio_get_value(S3C2410_GPB(0));
	copy_to_user(buff, (const void *)&pinval, 1); 
	printk("gpiob0 is %d\n" , pinval);
	return 0;
}
static ssize_t pwm_write(struct file *file, const char __user *buf, size_t count , loff_t * ppos)
{
	char val;

	copy_from_user(&val, buf, 1);
	printk("val is %d\n" , val);
	switch(val)
	{
		case 0 :*tcmpb0 = 52500 ;break;
		case 1 :*tcmpb0 = 62500>>1 ;break;
		case 2 :*tcmpb0 = 62500>>2 ;break;
		case 3 :*tcmpb0 = 62500>>3 ;break;
		default:break;
	}
	return 0;
}

/* 2. 构造file_operations */
static struct file_operations pwm_fops = {
	.owner	= THIS_MODULE,
	.open	= pwm_open,
	.read	= pwm_read,	   
	.write	= pwm_write,
	//.ioctl   =   s3c24xx_pwm_ioctl,static int s3c24xx_pwm_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)  ioctl(fd, 1);
};

static int pwm_init(void)
{
	int res;
	struct device *pwm_res;
	dev_t devid;

	/* 3. 告诉内核 */
#if 0
	major = register_chrdev(0, "hello", &hello_fops); /* (major,  0), (major, 1), ..., (major, 255)都对应hello_fops */
#else /*仅仅是注册设备号*/
	if (major) {
		devid = MKDEV(major, 0);
		register_chrdev_region(devid, 1, "my_pwm");  /* (major,0) 对应 pwm_fops, (major, 1~255)都不对应pwm_fops */
	} else {
		alloc_chrdev_region(&devid, 0, 1, "my_pwm"); /* (major,0) 对应 pwm_fops, (major, 1~255)都不对应pwm_fops */
		major = MAJOR(devid);                     
	}
	
	cdev_init(&pwm_cdev, &pwm_fops);
	res=cdev_add(&pwm_cdev, devid, 1);
	if(res)
	{
		printk("cdev_add failed\n");
		return 0;
	}
#endif

	cls = class_create(THIS_MODULE, "my_pwm_class");
	pwm_res = device_create(cls, NULL, MKDEV(major, 0), NULL, "pwm"); /* /dev/pwm */
	if (IS_ERR(pwm_res)) 
	{
		printk("device_create failed\n");
		return 0;
	}

	gpbcon 	= ioremap(0x56000010, 8);
	gpbdat 	= gpbcon+1;
	tcfg0	= ioremap(0x51000000, 20);
	tcfg1 	= tcfg0+1;
	tcon		= tcfg0+2;
	tcntb0	= tcfg0+3;
	tcmpb0	= tcfg0+4;
	if (!gpbcon)
	{
		printk("ioremap failed\n");
		return -EIO;
	}
	return 0;
}

static void pwm_exit(void)
{
	device_destroy(cls, MKDEV(major, 0));//class_device_destroy(cls,MKDEV(major, 0));

	class_destroy(cls);

	cdev_del(&pwm_cdev);
	unregister_chrdev_region(MKDEV(major, 0), 1);

	iounmap(gpbcon);
	iounmap(tcfg0);
}

module_init(pwm_init);
module_exit(pwm_exit);


MODULE_LICENSE("GPL");

/* 描述驱动程序的一些信息，不是必须的 */
MODULE_AUTHOR("kite");
MODULE_VERSION("0.1.0");
MODULE_DESCRIPTION("S3C2440 pwm Driver");


