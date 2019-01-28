#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <sound/core.h>
#include <linux/spi/spi.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>

#include <mach/hardware.h>
#include <mach/regs-gpio.h>

#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-samsung.h>
#include <mach/regs-gpio.h>
#include <linux/delay.h>

/* 1. 确定主设备号 */
static int major;
static struct cdev spi_cdev;
static struct class *cls;

#define SET_MOSI	gpio_set_value(S3C2410_GPG(6), 1)
#define CLR_MOSI	gpio_set_value(S3C2410_GPG(6), 0)
#define SET_SCLK		gpio_set_value(S3C2410_GPG(7), 1)
#define CLR_SCLK	gpio_set_value(S3C2410_GPG(7), 0)
#define SET_CSN		gpio_set_value(S3C2410_GPG(2), 1)
#define CLR_CSN		gpio_set_value(S3C2410_GPG(2), 0)

#define READMISO	gpio_get_value(S3C2410_GPG(5))
//gpio_set_value(S3C2410_GPB(0), 0);gpio_get_value(S3C2410_GPB(0));

static volatile unsigned long *SPCON1;
static volatile unsigned long *SPSTA1;
static volatile unsigned long *SPPIN1;
static volatile unsigned long *SPPRE1;
static volatile unsigned long *SPTDAT1;
static volatile unsigned long *SPRDAT1;
static volatile unsigned long *CLKCO;

static u8 NRFSPI(u8 date)			//NRF24L01的SPI写时序
{
	u8 i;
	for(i=0;i<8;i++)          // 循环8次
	{
		if(date&0x80)
			SET_MOSI;//MOSI=1;
		else
			CLR_MOSI;//MOSI=0;   // byte最高位输出到MOSI
		date<<=1;             // 低一位移位到最高位
		SET_SCLK;//SCLK=1; 
		if(READMISO)			//读取MOSO// 拉高SCK，nRF24L01从MOSI读入1位数据，同时从MISO输出1位数据
			date|=0x01;       	// 读MISO到byte最低位
		CLR_SCLK;//SCLK=0;            	// SCK置低
   	}
	return(date);           	// 返回读出的一字节
}

static u8 SPI_WriteReg(u8 RegAddr,u8 date)
{
	u8 BackDate;
	CLR_CSN;//CSN=0;//启动时序
	BackDate=NRFSPI(RegAddr);//写入地址
	NRFSPI(date);//写入值
	SET_CSN;//CSN=1;  
	return(BackDate);
}

static u8 SPI_ReadReg(u8 RegAddr)
{
	u8 BackDate;
	CLR_CSN;//CSN=0;//启动时序
	NRFSPI(RegAddr);//写寄存器地址
	BackDate=NRFSPI(0x00);//写入读寄存器指令  
	SET_CSN;//CSN=1;
	return(BackDate); //返回状态
}

#define SPI_TXRX_READY      (((*SPSTA1)&0x1) )

static int s3c24xx_spi_open(struct inode *inode, struct file *file)
{
	u8 val;
	printk("this is open \n");
#if 0
	gpio_set_value(S3C2410_GPF(1), 0);//ce=0
	SET_CSN;
	CLR_SCLK;
	SPI_WriteReg(0x30,0xA5);
	val = SPI_ReadReg(0x10);
	gpio_set_value(S3C2410_GPF(1), 1);//ce=1
	printk("val = %d \n" , val);
#else
	gpio_set_value(S3C2410_GPF(1), 0);//ce=0
	CLR_CSN;//CSN=0;
	*SPTDAT1 = 0x30;
	mdelay(1);
	while(!SPI_TXRX_READY)
		mdelay(1);

	*SPTDAT1 = 0xA5;
	mdelay(1);
	while(!SPI_TXRX_READY)
		mdelay(1);
	SET_CSN;//CSN=1; 
	
	CLR_CSN;//CSN=0;
	*SPTDAT1 = 0x10;
	mdelay(1);
	while(!SPI_TXRX_READY)
		mdelay(1);

	*SPTDAT1 = 0x00;
	mdelay(1);
	while(!SPI_TXRX_READY)
		mdelay(1);
	val = *SPRDAT1;
	SET_CSN;//CSN=1; 
	gpio_set_value(S3C2410_GPF(1), 1);//ce=1
	printk("val = %d \n" , val);
#endif
	
	return 0;
}

static int s3c24xx_spi_read(struct file *filp, char __user *buff, size_t count , loff_t *offp)
{
	return 0;
}

static struct file_operations s3c24xx_spi_fops = {
	.owner	=   THIS_MODULE,    /* 这是一个宏，推向编译模块时自动创建的__this_module变量 */
	.open	=	s3c24xx_spi_open,     
	.read	=	s3c24xx_spi_read,	   
	//.write	=	s3c24xx_leds_write,	   
};

static int spi_init(void)
{
	int res;
	struct device *spi_res;
	dev_t devid;

	/* 3. 告诉内核 */
#if 0
	major = register_chrdev(0, "hello", &hello_fops); /* (major,  0), (major, 1), ..., (major, 255)都对应hello_fops */
#else /*仅仅是注册设备号*/
	if (major) {
		devid = MKDEV(major, 0);
		register_chrdev_region(devid, 1, "my_spi");  /* (major,0) 对应 pwm_fops, (major, 1~255)都不对应pwm_fops */
	} else {
		alloc_chrdev_region(&devid, 0, 1, "my_spi"); /* (major,0) 对应 pwm_fops, (major, 1~255)都不对应pwm_fops */
		major = MAJOR(devid);                     
	}
	
	cdev_init(&spi_cdev, &s3c24xx_spi_fops);
	res=cdev_add(&spi_cdev, devid, 1);
	if(res)
	{
		printk("cdev_add failed\n");
		return 0;
	}
#endif

	cls = class_create(THIS_MODULE, "my_spi_class");
	spi_res = device_create(cls, NULL, MKDEV(major, 0), NULL, "spi"); /* /dev/pwm */
	if (IS_ERR(spi_res)) 
	{
		printk("device_create failed\n");
		return 0;
	}
	
	s3c_gpio_cfgpin(S3C2410_GPF(1), S3C_GPIO_OUTPUT );
	s3c_gpio_cfgpin(S3C2410_GPG(2), S3C_GPIO_OUTPUT );
#if 0
	s3c_gpio_cfgpin(S3C2410_GPG(5), S3C_GPIO_INPUT );
	s3c_gpio_cfgpin(S3C2410_GPG(6), S3C_GPIO_OUTPUT );
	s3c_gpio_cfgpin(S3C2410_GPG(7), S3C_GPIO_OUTPUT );
#else
	s3c_gpio_cfgpin(S3C2410_GPG(5), S3C2410_GPG5_SPIMISO1);
	s3c_gpio_cfgpin(S3C2410_GPG(6), S3C2410_GPG6_SPIMOSI1);
	s3c_gpio_cfgpin(S3C2410_GPG(7), S3C2410_GPG7_SPICLK1);

	//CLKCO 	= ioremap(0x4C00000C, 8);
	//*CLKCO = ((*CLKCO) &~ (1<<18))|(1 << 18);
	
	SPCON1 	= ioremap(0x59000020, 16);
	SPSTA1  = SPCON1+1;
	SPPIN1 	= SPSTA1+1;
	SPPRE1 	= SPPIN1+1;//0x18
	SPTDAT1 	= ioremap(0x59000030, 8);
	SPRDAT1 = SPTDAT1+1;

	*SPCON1 = (0 << 0 | 0 << 1 | 0 << 2 | 1 << 3 | 1 << 4 | 0 << 5);
	*SPPIN1 = (0<<2)|(0<<0);
	*SPPRE1 = 0x18;
#endif

    	return 0;
}

static void spi_exit(void)
{
    //spi_unregister_driver(&spi_rc522_drv);
}

module_init(spi_init);
module_exit(spi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kite");/*modinfo my_keyboard.ko*/
MODULE_DESCRIPTION("A spi-rc522 Module for testing module ");
MODULE_VERSION("V1.0");

