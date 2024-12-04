/*
 * Copyright (c) 2024 Zhang YiXu
 *
 * This code is for personal learning and open-source sharing purposes only.
 * Unauthorized use for commercial purposes is prohibited.
 *
 * Licensed under the MIT License. You may freely use, modify, and distribute
 * this code in compliance with the terms of the license.
 * 
 * Full text of the MIT License can be found at:
 * https://opensource.org/licenses/MIT
 *
 * Author: Zhang YiXu
 * Contact: zhangyixu02@gmail.com
 */

#include "linux/printk.h"
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>

void LED_release(struct device *dev)
{
	printk("LED_device_release\n");
}

struct resource led_resource[] = {
	{
		.start = 131,
		.end = 131,
		.flags = IORESOURCE_IRQ,
		.name  = "led0"
	},
	// {
	// 	.start = 132,
	// 	.end = 132,
	// 	.flags = IORESOURCE_IRQ,
	// 	.name = "led1",
	// },

};


struct platform_device led_device = {
	.name = "led_device",
	.id = 0,
	.resource = led_resource,
	.num_resources = ARRAY_SIZE(led_resource),
	.dev = {
		.release = LED_release,
	},
};


static int __init led_device_init(void)
{
    int ret;
	
    ret = platform_device_register(&led_device);
    if (ret < 0) {
        printk(KERN_ERR "Failed to register platform device\n");
        return ret;
    }

	printk("led_device_register ok \n");
	return 0;
}

static void __exit led_device_exit(void)
{
	platform_device_unregister(&led_device);
	printk("led_device_exit! \n");
}

module_init(led_device_init);
module_exit(led_device_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CSDN:qq_63922192");
MODULE_DESCRIPTION("LED device");
