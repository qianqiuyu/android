#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <linux/gpio.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-samsung.h>


static struct spi_board_info spi_info_rc5221[] = {
	{
    	 .modalias = "rc522",  /* 对应的spi_driver名字也是"rc522" */
    	 .max_speed_hz = 8000000,	/* max spi clock (SCK) speed in HZ */
    	 .bus_num = 1,     /* jz2440里接在SPI CONTROLLER 1 */
    	 .mode    = SPI_MODE_0,
    	 .chip_select   = S3C2410_GPG(2), /* oled_cs, 它的含义由spi_master确定 */
    	 //.platform_data = (const void *)S3C2410_GPG(4) , /* oled_dc, 它在spi_driver里使用 */    	 
	 },
};

static int spi_info_rc522_init(void)
{
	spi_register_board_info(spi_info_rc5221, ARRAY_SIZE(spi_info_rc5221));
	return 0;
}

static int spi_info_rc522_exit(void)
{
	//spi_unregister_device();
	return 0;
   
}
//EXPORT_SYMBOL(spi_register_board_info);

module_init(spi_info_rc522_init);
module_exit(spi_info_rc522_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kite");/*modinfo rc522.ko*/
MODULE_DESCRIPTION("A spi-rc522 Module for testing module ");
MODULE_VERSION("V1.0");
