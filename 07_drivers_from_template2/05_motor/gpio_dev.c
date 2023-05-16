#include <linux/module.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/gfp.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/timer.h>


static struct resource my_drv_resource[] = {
	{
        .start          = 115,
        .end            = 115,
        .flags          = IORESOURCE_IRQ,
	},
	{
        .start          = 116,
        .end            = 116,
        .flags          = IORESOURCE_IRQ,
	},
	{
        .start          = 117,
        .end            = 117,
        .flags          = IORESOURCE_IRQ,
	},
	{
        .start          = 118,
        .end            = 118,
        .flags          = IORESOURCE_IRQ,
	},
};

static struct platform_device gpio_platform_device = {
        .name           = "100ask_gpio_plat_drv",      //根据这个名字，找到驱动
        .id             = 0,                           //无所谓
        .num_resources  = ARRAY_SIZE(my_drv_resource), //资源个数，ARRAY_SIZE用于计算数组大小
        .resource       = my_drv_resource,             //用于描述硬件资源信息
};

static int __init gpio_dev_init(void)
{
	/* 注册platform_driver */
	return platform_device_register(&gpio_platform_device);  //注意，这里是device表示是设备
}

static void __exit gpio_dev_exit(void)
{
	/* 反注册platform_driver */
	platform_device_unregister(&gpio_platform_device);       //注意，这里是device表示是设备
}

/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(gpio_dev_init);  //入口函数
module_exit(gpio_dev_exit);  //出口函数

MODULE_LICENSE("GPL");


