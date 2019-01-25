#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>

#include <asm/mach/map.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include "mpu6050.h"


/* 1. 确定主设备号 */
static int major;
static struct cdev mpu6050_cdev;
static struct class *cls;

static struct i2c_client * mpu6050_client;


static int mpu6050_read_len(struct i2c_client * client, unsigned char reg_add , unsigned char len, unsigned char *buf)
{
    int ret;

    /* 要读取的那个寄存器的地址 */
    char txbuf = reg_add;

    struct i2c_msg msg[] = {
        {client->addr, 0, 1, &txbuf},       //0表示写，
        {client->addr, I2C_M_RD, len, buf}, //读数据
    };

    /* 通过i2c_transfer函数操作msg */
    ret = i2c_transfer(client->adapter, msg, 2);    //执行2条msg
    if (ret < 0)
    {
        printk("i2c_transfer read err\n");
        return -1;
    }

    return 0;
}


static int mpu6050_read_byte(struct i2c_client * client, unsigned char reg_add)
{
    int ret;

    /* 要读取的那个寄存器的地址 */
    char txbuf = reg_add;

    /* 用来接收读到的数据 */
    char rxbuf[1];

    /* i2c_msg指明要操作的从机地址，方向，长度，缓冲区 */
    struct i2c_msg msg[] = {
        {client->addr, 0, 1, &txbuf},       //0表示写，
        {client->addr, I2C_M_RD, 1, rxbuf}, //读数据
    };

    /* 通过i2c_transfer函数操作msg */
    ret = i2c_transfer(client->adapter, msg, 2);    //执行2条msg
    if (ret < 0)
    {
        printk("i2c_transfer read err\n");
        return -1;
    }

    return rxbuf[0];
}

static int mpu6050_write_byte(struct i2c_client * client, unsigned char reg_addr, unsigned char data)
{
    int ret;

    /* 要写的那个寄存器的地址和要写的数据 */
    char txbuf[] = {reg_addr, data};

    struct i2c_msg msg[] = {
        {client->addr, 0, 2, txbuf}//0表示写
    };

    ret = i2c_transfer(client->adapter, msg, 1);
    if (ret < 0)
    {
        printk("i2c_transfer write err\n");
        return -1;
    }

    return 0;
}

static int mpu6050_open(struct inode *inode, struct file *file)   
{
	char res;
	
	printk("%s called\n", __func__);

	mpu6050_write_byte(mpu6050_client, MPU_PWR_MGMT1_REG, 0X80);/*复位MPU6050*/
	mdelay(100);
	mpu6050_write_byte(mpu6050_client, MPU_PWR_MGMT1_REG, 0X00);
    	mpu6050_write_byte(mpu6050_client, MPU_GYRO_CFG_REG, 3<<3);/*陀螺仪传感器,±2000dps*/
    	mpu6050_write_byte(mpu6050_client, MPU_ACCEL_CFG_REG, 0<<3);/*加速度传感器,±2g*/
    	mpu6050_write_byte(mpu6050_client, MPU_SAMPLE_RATE_REG, 1000 /50-1);/*设置采样率50Hz*/
    	mpu6050_write_byte(mpu6050_client, MPU_CFG_REG, 4);/*自动设置LPF为采样率的一半*/
	mpu6050_write_byte(mpu6050_client, MPU_INT_EN_REG, 0X00);/*关闭所有中断*/
	mpu6050_write_byte(mpu6050_client, MPU_USER_CTRL_REG, 0X00);/*I2C主模式关闭*/
	mpu6050_write_byte(mpu6050_client, MPU_FIFO_EN_REG, 0X00);/*关闭FIFO*/
	mpu6050_write_byte(mpu6050_client, MPU_INTBP_CFG_REG, 0X80);/*INT引脚低电平有效*/
	
	res = mpu6050_read_byte(mpu6050_client, MPU_DEVICE_ID_REG);
	mpu6050_write_byte(mpu6050_client, MPU_CFG_REG, 3);//设置数字低通滤波器
	if (res == MPU_ADDR)//器件ID正确
	{
		printk("I2C ID is right ! \n");
		mpu6050_write_byte(mpu6050_client, MPU_PWR_MGMT1_REG, 0X01);	/*设置CLKSEL,PLL X轴为参考*/
		mpu6050_write_byte(mpu6050_client, MPU_PWR_MGMT2_REG, 0X00);	/*加速度与陀螺仪都工作*/
		return 0;
	}
	printk("failed !I2C ID is error ! \n");
	return 0;  
}  

static ssize_t mpu6050_read(struct file * file, char __user *buf, size_t count, loff_t *off)
{
	char val;
	unsigned char rxbuf[6], res;
	copy_from_user(&val, buf, 1);
	res = mpu6050_read_len(mpu6050_client, MPU_ACCEL_XOUTH_REG, 6 , rxbuf);
	if (res == 0)/* 加速度计原始数据   */
	{
		printk("ax = %d \n", ((u16)rxbuf[0] << 8) | rxbuf[1]);
		printk("ay = %d \n", ((u16)rxbuf[2] << 8) | rxbuf[3]);
		printk("az = %d \n", ((u16)rxbuf[4] << 8) | rxbuf[5]);
	}
	res = mpu6050_read_len(mpu6050_client, MPU_GYRO_XOUTH_REG, 6 , rxbuf);
	if (res == 0)/*陀螺仪原始数据*/
	{
		printk("gx = %d \n", ((u16)rxbuf[0] << 8) | rxbuf[1]);
		printk("gy = %d \n", ((u16)rxbuf[2] << 8) | rxbuf[3]);
		printk("gz = %d \n", ((u16)rxbuf[4] << 8) | rxbuf[5]);
	}
	return 0;
}
static ssize_t mpu6050_write(struct file *file, const char __user *buf, size_t count , loff_t * ppos)
{
	return 0;
}
static long mpu6050_ioctl(struct file *file, unsigned int cmd, unsigned long arg)  
{
	return 0;
}

/* 2. 构造file_operations */
static struct file_operations mpu6050_fops = {
	.owner	= THIS_MODULE,
	.open	= mpu6050_open,
	.read	= mpu6050_read,	   
	.write	= mpu6050_write,
	.unlocked_ioctl   =   mpu6050_ioctl,
};

static int mpu6050_probe(struct i2c_client *client, const struct i2c_device_id *id)  
{
	int res;
	struct device *mpu6050_res;
	dev_t devid;

	mpu6050_client = client;
	
	/* 3. 告诉内核 */
#if 0
	major = register_chrdev(0, "hello", &hello_fops); /* (major,  0), (major, 1), ..., (major, 255)都对应hello_fops */
#else /*仅仅是注册设备号*/
	if (major) {
		devid = MKDEV(major, 0);
		register_chrdev_region(devid, 1, "mpu6050");  /* (major,0) 对应 pwm_fops, (major, 1~255)都不对应pwm_fops */
	} else {
		alloc_chrdev_region(&devid, 0, 1, "mpu6050"); /* (major,0) 对应 pwm_fops, (major, 1~255)都不对应pwm_fops */
		major = MAJOR(devid);                     
	}
	
	cdev_init(&mpu6050_cdev, &mpu6050_fops);
	res=cdev_add(&mpu6050_cdev, devid, 1);
	if(res)
	{
		printk("cdev_add failed\n");
		unregister_chrdev_region(MKDEV(major, 0), 1);
		return 0;
	}
#endif
	cls = class_create(THIS_MODULE, "mpu6050");
	mpu6050_res = device_create(cls, NULL, MKDEV(major, 0), NULL, "mpu6050"); /* /dev/xxx */
	if (IS_ERR(mpu6050_res)) 
	{
		printk("device_create failed\n");
		return 0;
	}

	return 0;	
}
static int mpu6050_remove(struct i2c_client *client)  
{  
	device_destroy(cls, MKDEV(major, 0));//class_device_destroy(cls,MKDEV(major, 0));

	class_destroy(cls);
	
	cdev_del(&mpu6050_cdev);
	unregister_chrdev_region(MKDEV(major, 0), 1);   
  
    return 0;  
}

static const struct i2c_device_id mpu6050_id[] = {  
    { "mpu6050", 0},  
    {}  
};

struct i2c_driver mpu6050_driver = {  
    .driver = {  
        .name           = "mpu6050",  
        .owner          = THIS_MODULE,  
    },  
    .probe      = mpu6050_probe,  
    .remove     = mpu6050_remove,  
    .id_table   = mpu6050_id,  
};

static int I2C_mpu6050_init(void)
{
	return i2c_add_driver(&mpu6050_driver);
}

static void I2C_mpu6050_exit(void)
{
	return i2c_del_driver(&mpu6050_driver);
}

module_init(I2C_mpu6050_init);
module_exit(I2C_mpu6050_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kite");/*modinfo my_keyboard.ko*/
MODULE_DESCRIPTION("A i2c-mpu6050 Module for testing module ");
MODULE_VERSION("V1.0");
