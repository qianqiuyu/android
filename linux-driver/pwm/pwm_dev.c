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
#include <clocksource/samsung_pwm.h>
#include <plat/pwm-core.h>

static volatile unsigned long *gpbcon;
static volatile unsigned long *gpbdat;

static struct samsung_pwm_variant s3c_pwm_pdata = {
	.bits		= 16,
	.div_base	= 1,
	.has_tint_cstat	= false,
	.tclk_mask	= (1 << 4),
	.output_mask	= 1,
};

static struct resource s3c_pwm_resource[] = {
	DEFINE_RES_MEM(SAMSUNG_PA_TIMER, SZ_4K),
};

static struct platform_device s3c_device_pwm = {/*platform里注册设备文件*/
	.name			= "samsung-pwm",
	.id				= -1,
	.num_resources	= ARRAY_SIZE(s3c_pwm_resource),
	.resource	= s3c_pwm_resource,
	.dev				= {
		.platform_data	= &s3c_pwm_pdata,
	},
};

static int S3C_PWM_init(void)
{
	/* 2. 注册 */
	int result = platform_device_register(&s3c_device_pwm);
	if (result != 0) /*注册失败*/ 
		printk("register false \n");
	gpbcon 	= ioremap(0x56000010, 8);
	gpbdat 	= gpbcon+1;

	*gpbcon &= ~(0x3<<(0*2));
	*gpbcon |= (0x2<<(0*2));					/*复用模式*/
	return 0;
}

static void S3C_PWM_exit(void)
{
	platform_device_unregister(&s3c_device_pwm);
	iounmap(gpbcon);
}

module_init(S3C_PWM_init);
module_exit(S3C_PWM_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kite");/*modinfo my_keyboard.ko*/
MODULE_DESCRIPTION("A pwm Module for testing module ");
MODULE_VERSION("V1.0");
