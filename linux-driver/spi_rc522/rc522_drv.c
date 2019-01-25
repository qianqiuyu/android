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
#include <linux/delay.h>

#include "rc522.h"
#include <linux/gpio.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-samsung.h>

static volatile unsigned long *gpfcon;
static volatile unsigned long *gpfdat;//f1
#define SET_RC522RST *gpfdat |= (0x01<<(1));
#define CLR_RC522RST *gpfdat &= ~(0x01<<(1));

static struct spi_device *spi_dev;

/* 1. 确定主设备号 */
static int major;
static struct cdev rc522_cdev;
static struct class *cls;

u8 ReadRawRC(u8   Address )
{
	unsigned char tx_buf[1];
    	unsigned char rx_buf[1];
    
	tx_buf[0] = ((Address<<1)&0x7E)|0x80;//RC522 set

	spi_write_then_read(spi_dev, tx_buf, 1, rx_buf, 1);

	return rx_buf[0];
}

void WriteRawRC(u8   Address, u8   value)
{
	u8   ucAddr[2];

	ucAddr[0] = ((Address<<1)&0x7E);//RC522 set
	ucAddr[1] = value;
	
	spi_write(spi_dev, ucAddr, 2);
}

void ClearBitMask(u8   reg,u8   mask)  
{
    char   tmp = 0x0;
    tmp = ReadRawRC(reg);
    WriteRawRC(reg, tmp & ~mask);  // clear bit mask
}

void SetBitMask(u8   reg,u8   mask)  
{
	char   tmp = 0x0;
	tmp = ReadRawRC(reg);
	WriteRawRC(reg,tmp | mask);  // set bit mask
}

static int rc522_open(struct inode *inode, struct file *file)   
{
	u8   i;u8  Re=0;
	printk("this is open \n");
	
	/*复位*/
	SET_RC522RST;
	ndelay(10);
	CLR_RC522RST;
	ndelay(10);
	SET_RC522RST;
	ndelay(10);
    	WriteRawRC(CommandReg,PCD_RESETPHASE);
	WriteRawRC(CommandReg,PCD_RESETPHASE);
    	ndelay(10);

	WriteRawRC(ModeReg,0x3D);            //和Mifare卡通讯，CRC初始值0x6363
	WriteRawRC(TReloadRegL,30);           
    	WriteRawRC(TReloadRegH,0);
    	WriteRawRC(TModeReg,0x8D);
    	WriteRawRC(TPrescalerReg,0x3E);	
	WriteRawRC(TxAutoReg,0x40);//必须要

	/*关闭天线*/
	ClearBitMask(TxControlReg, 0x03);//

	mdelay(2);
	
	/*开启天线*/
	i = ReadRawRC(TxControlReg);
   	 if (!(i & 0x03))
    	{
        	SetBitMask(TxControlReg, 0x03);
    	}

	/*设置RC632的工作方式 */
	ClearBitMask(Status2Reg,0x08);
       WriteRawRC(ModeReg,0x3D);//3F
       WriteRawRC(RxSelReg,0x86);//84
       WriteRawRC(RFCfgReg,0x7F);   //4F
	WriteRawRC(TReloadRegL,30);//tmoLength);// TReloadVal = 'h6a =tmoLength(dec) 
	WriteRawRC(TReloadRegH,0);
       WriteRawRC(TModeReg,0x8D);
	WriteRawRC(TPrescalerReg,0x3E);
	ndelay(1000);
       /*开启天线*/
	i = ReadRawRC(TxControlReg);
   	 if (!(i & 0x03))
    	{
        	SetBitMask(TxControlReg, 0x03);
    	}
	
	return 0;
}

char PcdComMF522(u8   Command, 
                 u8 *pIn , 
                 u8   InLenByte,
                 u8 *pOut , 
                 u8 *pOutLenBit)
{
    char   status = MI_ERR;
    u8   irqEn   = 0x00;
    u8   waitFor = 0x00;
    u8   lastBits;
    u8   n;
    u16   i;
    switch (Command)
    {
        case PCD_AUTHENT:
			irqEn   = 0x12;
			waitFor = 0x10;
			break;
		case PCD_TRANSCEIVE:
			irqEn   = 0x77;
			waitFor = 0x30;
			break;
		default:
			break;
    }
   
    WriteRawRC(ComIEnReg,irqEn|0x80);
    ClearBitMask(ComIrqReg,0x80);	//清所有中断位
    WriteRawRC(CommandReg,PCD_IDLE);
    SetBitMask(FIFOLevelReg,0x80);	 	//清FIFO缓存
    
    for (i=0; i<InLenByte; i++)
    {   WriteRawRC(FIFODataReg, pIn [i]);    }
    WriteRawRC(CommandReg, Command);	  
    
    if (Command == PCD_TRANSCEIVE)
    {    SetBitMask(BitFramingReg,0x80);  }	 //开始传送
    										 
    //i = 600;//根据时钟频率调整，操作M1卡最大等待时间25ms
	i = 60;
    do 
    {
        n = ReadRawRC(ComIrqReg);
        i--;mdelay(50);
    }
    while ((i!=0) && !(n&0x01) && !(n&waitFor));
    ClearBitMask(BitFramingReg,0x80);

    if (i!=0)
    {    
        if(!(ReadRawRC(ErrorReg)&0x1B))
        {
            status = MI_OK;
            if (n & irqEn & 0x01)
            {   status = MI_NOTAGERR;   }
            if (Command == PCD_TRANSCEIVE)
            {
               	n = ReadRawRC(FIFOLevelReg);
              	lastBits = ReadRawRC(ControlReg) & 0x07;
                if (lastBits)
                {   *pOutLenBit = (n-1)*8 + lastBits;   }
                else
                {   *pOutLenBit = n*8;   }
                if (n == 0)
                {   n = 1;    }
                if (n > MAXRLEN)
                {   n = MAXRLEN;   }
                for (i=0; i<n; i++)
                {   pOut [i] = ReadRawRC(FIFODataReg);    }
            }
        }
        else
        {   status = MI_ERR;   }
        
    }
   

    SetBitMask(ControlReg,0x80);           // stop timer now
    WriteRawRC(CommandReg,PCD_IDLE); 
    return status;
}


char PcdRequest(u8   req_code,u8 *pTagType)
{
	char   status;  
	u8   unLen;
	u8   ucComMF522Buf[MAXRLEN]; 

	ClearBitMask(Status2Reg,0x08);
	WriteRawRC(BitFramingReg,0x07);
	SetBitMask(TxControlReg,0x03);
 
	ucComMF522Buf[0] = req_code;

	status = PcdComMF522(PCD_TRANSCEIVE,ucComMF522Buf,1,ucComMF522Buf,&unLen);

	if ((status == MI_OK) && (unLen == 0x10))
	{    
		*pTagType     = ucComMF522Buf[0];
		*(pTagType+1) = ucComMF522Buf[1];
	}
	else
	{  
		status = MI_ERR;  
	}
   
	return status;
}

char PcdAnticoll(u8 *pSnr)
{
	char   status;
	u8   i,snr_check=0;
	u8   unLen;
	u8   ucComMF522Buf[MAXRLEN];   

	ClearBitMask(Status2Reg,0x08);
	WriteRawRC(BitFramingReg,0x00);
	ClearBitMask(CollReg,0x80);
 
	ucComMF522Buf[0] = PICC_ANTICOLL1;
	ucComMF522Buf[1] = 0x20;

	status = PcdComMF522(PCD_TRANSCEIVE,ucComMF522Buf,2,ucComMF522Buf,&unLen);

	if (status == MI_OK)
	{
		for (i=0; i<4; i++)
		{   
			*(pSnr+i)  = ucComMF522Buf[i];
			snr_check ^= ucComMF522Buf[i];
		}
		if (snr_check != ucComMF522Buf[i])
		{  
			status = MI_ERR;    
		}
    }
    
    SetBitMask(CollReg,0x80);
    return status;
}

static ssize_t rc522_read(struct file * file, char __user *buf, size_t count, loff_t *off)
{
	u8 status;
	u8 CT[2];//卡类型
	u8 UID[4]; //卡号
	status = PcdRequest(PICC_REQALL,CT);/*尋卡 输出为卡类型----CT卡类型*/
	if(status==MI_OK)
	{
		printk(" find car ok \n");
		status=MI_ERR;
		status = PcdAnticoll(UID);/*防冲撞*/	
		if (status==MI_OK)//防衝撞成功
		{
			status=MI_ERR;	
			printk("The car id is %d%d%d%d \n" , UID[0],UID[1],UID[2],UID[3]);
		}
		
	}
	else
		printk("find car faid \n");
	
		
	return 0;
}

static ssize_t rc522_write(struct file *file, const char __user *buf,size_t count, loff_t *ppos)
{
	return 0;
}

static struct file_operations rc522_fops = {
	.owner	= THIS_MODULE,
	.open	= rc522_open,
    	.read	= rc522_read, 
	.write	= rc522_write,
};

static int spi_rc522_probe(struct spi_device *spi)
{
	int res;
	struct device *rc522_res;
	dev_t devid;
	s3c_gpio_cfgpin(S3C2410_GPG(2), S3C_GPIO_OUTPUT);//
	spi_dev = spi;
	/* 3. 告诉内核 */
#if 0
    major = register_chrdev(0, "hello", &hello_fops); /* (major,  0), (major, 1), ..., (major, 255)都对应hello_fops */
#else /*仅仅是注册设备号*/
    if (major) {
        devid = MKDEV(major, 0);
        register_chrdev_region(devid, 1, "rc522");  /* (major,0) 对应 pwm_fops, (major, 1~255)都不对应pwm_fops */
    } else {
        alloc_chrdev_region(&devid, 0, 1, "rc522"); /* (major,0) 对应 pwm_fops, (major, 1~255)都不对应pwm_fops */
        major = MAJOR(devid);                     
    }

    cdev_init(&rc522_cdev, &rc522_fops);
    res=cdev_add(&rc522_cdev, devid, 1);
    if(res)
    {
        printk("cdev_add failed\n");
        unregister_chrdev_region(MKDEV(major, 0), 1);
        return 0;
    }
#endif
    cls = class_create(THIS_MODULE, "rc522");
    rc522_res = device_create(cls, NULL, MKDEV(major, 0), NULL, "rc522"); /* /dev/xxx */
    if (IS_ERR(rc522_res)) 
    {
        printk("device_create failed\n");
        return 0;
    }

	gpfcon 	= ioremap(0x56000050 , 8);
	gpfdat 	= gpfcon+1;
	if (!gpfcon)
	{
		printk("ioremap failed\n");
		return -EIO;
	}

    return 0;
}


static struct spi_driver spi_rc522_drv = {
	.driver = {
		.name	= "rc522",
		.owner	= THIS_MODULE,
	},
	.probe		= spi_rc522_probe,
	//.remove		= spi_rc522_remove,
};

static int spi_rc522_init(void)
{
    return spi_register_driver(&spi_rc522_drv);
}

static void spi_rc522_exit(void)
{
    spi_unregister_driver(&spi_rc522_drv);
}

module_init(spi_rc522_init);
module_exit(spi_rc522_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kite");/*modinfo my_keyboard.ko*/
MODULE_DESCRIPTION("A spi-rc522 Module for testing module ");
MODULE_VERSION("V1.0");

