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
#include "linux/types.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/delay.h>

#define GPIO_NAME_MAX_LEN  32

struct gpio_desc {
	int gpio;
    char name[GPIO_NAME_MAX_LEN];
};

typedef struct chr_drv {
	int major;                 /*< Master device number */
	int minor;                 /*< Secondary device number */
	dev_t dev_num;             /*< Device number of type dev_t consisting of primary device number and secondary device number */
	struct cdev chr_cdev;      /*< Character device structure */
	struct class *chr_class;   /*< Device class */
    struct device *chr_device; /*< Device instance */
    struct gpio_desc *gpios;   /*< GPIO pin descriptor */
    int gpio_count;
}chr_drv;

static chr_drv s_char_drv;

/* 马达引脚设置数字 */
static int g_motor_pin_ctrl[8]= {0x2,0x3,0x1,0x9,0x8,0xc,0x4,0x6};
static int g_motor_index = 0;

void set_pins_for_motor(int index)
{
	int i;
	for (i = 0; i < 4; i++) {
		gpio_set_value(s_char_drv.gpios[i].gpio, g_motor_pin_ctrl[index] & (1<<i) ? 1 : 0);
	}
}

void disable_motor(void)
{
	int i;
	for (i = 0; i < 4; i++) {
		gpio_set_value(s_char_drv.gpios[i].gpio, 0);
	}
}

/* int buf[2];
 * buf[0] = 步进的次数, > 0 : 逆时针步进; < 0 : 顺时针步进
 * buf[1] = mdelay的时间
 */
static ssize_t chrdev_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    int ker_buf[2];
    int step;
    // chr_drv *chrdev_private_data = (chr_drv *)file->private_data;

    if (size != 8) {
        printk("wirte size is not 8 \n");
        return -EINVAL;
    }

    if (copy_from_user(ker_buf, buf, size) != 0) {
        printk("copy_from_user error\r\n");
        return -1;
    }

	if (ker_buf[0] > 0) {
		for (step = 0; step < ker_buf[0]; step++) {
			set_pins_for_motor(g_motor_index);
			// msleep(ker_buf[1]);
			mdelay(ker_buf[1]);
			g_motor_index--;
			if (g_motor_index == -1){
				g_motor_index = 7;
            }
		}
	}
	else {
		ker_buf[0] = 0 - ker_buf[0];
		for (step = 0; step < ker_buf[0]; step++) {
			set_pins_for_motor(g_motor_index);
			// msleep(ker_buf[1]);
			mdelay(ker_buf[1]);
			g_motor_index++;
			if (g_motor_index == 8) {
				g_motor_index = 0;
            }
		}
	}
	disable_motor();
    return 8;    
}

static int chrdev_open(struct inode *inode, struct file *file)
{
    int err,i;
    file->private_data = &s_char_drv;

	for (i = 0; i < s_char_drv.gpio_count; i++) {
		err = gpio_request(s_char_drv.gpios[i].gpio, s_char_drv.gpios[i].name);
		if (err < 0) {
			printk("can not request gpio %s %d\n", s_char_drv.gpios[i].name, s_char_drv.gpios[i].gpio);
			return -ENODEV;
		}
		gpio_direction_output(s_char_drv.gpios[i].gpio, 1);
	}

	printk("%s line %d\n", __FUNCTION__, __LINE__);
	return 0;
}

static int chrdev_release(struct inode *inode, struct file *file)
{
    int i;
    chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    for (i = 0; i < chrdev_private_data->gpio_count; i++) {
		gpio_free(chrdev_private_data->gpios[i].gpio);		
	}
	printk("%s line %d\n", __FUNCTION__, __LINE__);
	return 0;
}

static struct file_operations chr_file_operations = {
	.owner = THIS_MODULE,
	.write = chrdev_write,
	.open = chrdev_open,
	.release = chrdev_release,
};

static int key_drver_probe(struct platform_device *pdev)
{
	int ret,count,i;
    struct device_node *np = pdev->dev.of_node;

	/*< 1. Request equipment number */
    ret = alloc_chrdev_region(&s_char_drv.dev_num,0,1,"chrdev_num");
    if (ret < 0) {
        printk("alloc_chrdev_region is error\n");
        goto err_chrdev;
    }
    printk("major is %d\n",MAJOR(s_char_drv.dev_num));
    printk("minor is %d\n",MINOR(s_char_drv.dev_num));
	/*< 2. Registered character device */
    cdev_init(&s_char_drv.chr_cdev,&chr_file_operations);
	s_char_drv.chr_cdev.owner = THIS_MODULE;
    ret = cdev_add(&s_char_drv.chr_cdev,s_char_drv.dev_num,1);
    if(ret < 0 ){
        printk("cdev_add is error\n");
        goto  err_chr_add;
    }
    printk("Registered character device is ok\n");
	/*< 3. Creating a Device Node */
    s_char_drv.chr_class = class_create(THIS_MODULE,"chr_class_motor");
    if (IS_ERR(s_char_drv.chr_class)) {
        ret = PTR_ERR(s_char_drv.chr_device);
        printk("class_create failed with error code: %d\n", ret);
        goto err_class_create;
    }

    s_char_drv.chr_device = device_create(s_char_drv.chr_class,NULL,s_char_drv.dev_num,NULL,"chr_device_motor");
    if (IS_ERR(s_char_drv.chr_device)) {
        ret = PTR_ERR(s_char_drv.chr_device);
        printk("device_create failed with error code: %d\n", ret);
        goto err_device_create;
    }
    printk("creating a device node is ok\n");
    /*< 4. Obtaining device resources */
    if (np) {
        count = of_gpio_count(np);
        if (!count) {
            printk("Device tree: No matching device resources are available!\n");
            ret = -EINVAL;
            goto err_get_resource;
        }

        s_char_drv.gpio_count = count;
        s_char_drv.gpios = kmalloc(count * sizeof(struct gpio_desc), GFP_KERNEL);

        if (s_char_drv.gpios == NULL) {
            printk("Device tree: kmalloc is flaut\r\n");
            ret = -EINVAL;
            goto err_get_resource;
        }

        for (i = 0; i < count; i++) {
            s_char_drv.gpios[i].gpio = of_get_gpio(np, i);
            ret = sprintf(s_char_drv.gpios[i].name, "%s_%d", np->name,i);
            if (ret < 0) {
                printk("Device tree: sprintf is flaut \n");
                goto err_get_resource;
            }
            printk("gpios[%d].gpio is %d name is %s\n",i,s_char_drv.gpios[i].gpio,s_char_drv.gpios[i].name);
        }
    }
    printk("Obtaining device resources is ok\n");

    return 0;

err_get_resource:
    device_destroy(s_char_drv.chr_class,s_char_drv.dev_num);
err_device_create:
    class_destroy(s_char_drv.chr_class);
err_class_create:
    cdev_del(&s_char_drv.chr_cdev);
err_chr_add:
    unregister_chrdev_region(s_char_drv.dev_num, 1);
err_chrdev:
    return ret;
}

static int key_drver_remove(struct platform_device *pdev)
{
    device_destroy(s_char_drv.chr_class,s_char_drv.dev_num);
    class_destroy(s_char_drv.chr_class);
    cdev_del(&s_char_drv.chr_cdev);
    unregister_chrdev_region(s_char_drv.dev_num,1);

    printk("module exit \n");
    return 0;
}

static const struct of_device_id dts_table[] = 
{
	{.compatible = "motor"},
    { /* Sentinel (end of array marker) */ }
};

static struct platform_driver motor_driver = {
	.driver		= {
		.owner = THIS_MODULE,
        .name  = "motor_driver",
		.of_match_table = dts_table,
	},
	.probe		= key_drver_probe,
	.remove		= key_drver_remove,
};

static int __init chr_drv_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&motor_driver);
	if(ret<0)
	{
		printk("platform_driver_register error \n");
		return ret;
	}
	printk("platform_driver_register ok \n");
    return 0;
}

static void __exit chr_drv_exit(void)
{
	platform_driver_unregister(&motor_driver);
	printk("platform_driver_unregister ok \n");
}

module_init(chr_drv_init);
module_exit(chr_drv_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CSDN:qq_63922192");
MODULE_DESCRIPTION("LED driver");