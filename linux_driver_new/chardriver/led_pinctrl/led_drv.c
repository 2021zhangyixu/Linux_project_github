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


#define KBUF_MAX_SIZE      10
#define GPIO_NAME_MAX_LEN  32

struct gpio_desc {
	int gpio;
    char name[GPIO_NAME_MAX_LEN];
};


struct pinctrl_struct {
    struct pinctrl *pinctrl;
    struct pinctrl_state *led_on;
    struct pinctrl_state *led_off;
};

typedef struct chr_drv {
	dev_t dev_num;             /*< Device number of type dev_t consisting of primary device number and secondary device number */
	struct cdev chr_cdev;      /*< Character device structure */
	struct class *chr_class;   /*< Device class */
    struct device *chr_device; /*< Device instance */
    char kbuf[KBUF_MAX_SIZE];  /*< Kernel buffer */
    struct pinctrl_struct pinctrl_struct;   /*< Pinctrl descriptor */
    struct gpio_desc *gpios;   /*< GPIO pin descriptor */
    int gpio_count;
}chr_drv;

static chr_drv s_char_drv;

static int chrdev_open(struct inode *inode, struct file *file)
{
    // int err,i;
    file->private_data = &s_char_drv;
    // for (i = 0; i < s_char_drv.gpio_count; i++) {
	// 	err = gpio_request(s_char_drv.gpios[i].gpio, s_char_drv.gpios[i].name);
	// 	if (err < 0) {
	// 		printk("can not request gpio %s %d\n", s_char_drv.gpios[i].name, s_char_drv.gpios[i].gpio);
	// 		return -ENODEV;
	// 	}
	// 	gpio_direction_output(s_char_drv.gpios[i].gpio, 1);
	// }
	printk("%s line %d\n", __FUNCTION__, __LINE__);
	return 0;
}

static ssize_t chrdev_write(struct file *file,const char __user *buf,size_t size,loff_t *off)
{
    chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    if (copy_from_user(chrdev_private_data->kbuf, buf, size) != 0) {
        printk("copy_from_user error\r\n");
        return -1;
    }
    if (chrdev_private_data->kbuf[0]) {
        printk("on \n");
        pinctrl_select_state(chrdev_private_data->pinctrl_struct.pinctrl, chrdev_private_data->pinctrl_struct.led_on);
    } else {
        printk("off \n");
        pinctrl_select_state(chrdev_private_data->pinctrl_struct.pinctrl, chrdev_private_data->pinctrl_struct.led_off);
    }
    
	printk("%s line %d\n", __FUNCTION__, __LINE__);
	return size;
}

static int chrdev_release(struct inode *inode, struct file *file)
{
    // int i;
    // chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    // for (i = 0; i < chrdev_private_data->gpio_count; i++) {
	// 	gpio_free(chrdev_private_data->gpios[i].gpio);		
	// }
	printk("%s line %d\n", __FUNCTION__, __LINE__);
	return 0;
}

static struct file_operations chr_file_operations = {
	.owner = THIS_MODULE,
	.open = chrdev_open,
	.write = chrdev_write,
	.release = chrdev_release,
};

static int led_drver_probe(struct platform_device *pdev)
{
	int ret,count,i;
    struct device_node *np = pdev->dev.of_node;

	/*< 1. Request equipment number */
    ret = alloc_chrdev_region(&s_char_drv.dev_num,0,1,"chrdev_num");
    if (ret < 0) {
        printk("alloc_chrdev_region is error\n");
        goto err_chrdev;
    }
    printk("major is %d ,minor is %d \n",MAJOR(s_char_drv.dev_num),MINOR(s_char_drv.dev_num));
	/*< 2. Registered character device */
    cdev_init(&s_char_drv.chr_cdev,&chr_file_operations);
	s_char_drv.chr_cdev.owner = THIS_MODULE;
    ret = cdev_add(&s_char_drv.chr_cdev,s_char_drv.dev_num,1);
    if(ret < 0 ){
        printk("cdev_add is error\n");
        goto  err_chr_add;
    }
    printk("cdev_add is ok\n");
	/*< 3. Creating a Device Node */
    s_char_drv.chr_class = class_create(THIS_MODULE,"chr_class_led_pinctrl");
    if (IS_ERR(s_char_drv.chr_class)) {
        ret = PTR_ERR(s_char_drv.chr_device);
        printk("class_create failed with error code: %d\n", ret);
        goto err_class_create;
    }

    s_char_drv.chr_device = device_create(s_char_drv.chr_class,NULL,s_char_drv.dev_num,NULL,"chr_device_led_pinctrl");
    if (IS_ERR(s_char_drv.chr_device)) {
        ret = PTR_ERR(s_char_drv.chr_device);
        printk("device_create failed with error code: %d\n", ret);
        goto err_device_create;
    }
    /*< 4. Obtaining device resources */
    /*< gpio */
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
    /*< pinctrl*/
    s_char_drv.pinctrl_struct.pinctrl = devm_pinctrl_get(&pdev->dev);
    if (!s_char_drv.pinctrl_struct.pinctrl || IS_ERR(s_char_drv.pinctrl_struct.pinctrl)) {
        dev_err(&pdev->dev, "can't get pinctrl, bus recovery not supported\n");
        ret = PTR_ERR(s_char_drv.pinctrl_struct.pinctrl);
        goto err_get_resource;
    }
    s_char_drv.pinctrl_struct.led_on = pinctrl_lookup_state(s_char_drv.pinctrl_struct.pinctrl,"led_on");
    if (!s_char_drv.pinctrl_struct.led_on || IS_ERR(s_char_drv.pinctrl_struct.led_on)) {
        dev_err(&pdev->dev, "Failed to get pinctrl state 'led_on'\n");
        ret = PTR_ERR(s_char_drv.pinctrl_struct.led_on);
        goto err_get_resource;
    }
    s_char_drv.pinctrl_struct.led_off = pinctrl_lookup_state(s_char_drv.pinctrl_struct.pinctrl,"led_off");
    if (!s_char_drv.pinctrl_struct.led_off || IS_ERR(s_char_drv.pinctrl_struct.led_off)) {
        dev_err(&pdev->dev, "Failed to get pinctrl state 'led_off'\n");
        ret = PTR_ERR(s_char_drv.pinctrl_struct.led_off);
        goto err_get_resource;
    }
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

static int led_drver_remove(struct platform_device *pdev)
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
	{ .compatible = "dts_led@1", },
    { /* Sentinel (end of array marker) */ }
};

static struct platform_driver led_driver = {
	.driver		= {
		.owner = THIS_MODULE,
        .name  = "led_pinctrl_driver",
		.of_match_table = dts_table,
	},
	.probe		= led_drver_probe,
	.remove		= led_drver_remove,
};

static int __init chr_drv_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&led_driver);
	if(ret<0) {
		printk("platform_driver_register error \n");
		return ret;
	}
	printk("platform_driver_register ok \n");
    return 0;
}

static void __exit chr_drv_exit(void)
{
	platform_driver_unregister(&led_driver);
	printk("platform_driver_unregister ok \n");
}

module_init(chr_drv_init);
module_exit(chr_drv_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CSDN:qq_63922192");
MODULE_DESCRIPTION("LED Pinctrl driver");