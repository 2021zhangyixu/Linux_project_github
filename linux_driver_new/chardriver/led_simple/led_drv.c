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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#define KBUF_MAX_SIZE      32
#define GPIO_NAME_MAX_LEN  128

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
    char kbuf[KBUF_MAX_SIZE];  /*< Kernel buffer */
    struct gpio_desc *gpios;   /*< GPIO pin descriptor */
}chr_drv;

static chr_drv s_char_drv = {
    .gpios = (struct gpio_desc[]) {
        {131, "led0"},
    },
};

static int major = 0,minor = 0;
module_param(major,int,S_IRUGO); /*< Pass the major device number parameter as' insmod mymodule.ko major=10 ' */
MODULE_PARM_DESC(major, "Statically specify the primary device number.");
module_param(minor,int,S_IRUGO); /*< The minor device number parameter minor is passed as' insmod mymodule.ko minor=10 ' */
MODULE_PARM_DESC(minor, "Statically specify the secondary device number.The default value is 0");


static int chrdev_open(struct inode *inode, struct file *file)
{
    int err,i;
    int count = sizeof(s_char_drv)/sizeof(s_char_drv.gpios[0]);
    file->private_data = &s_char_drv;

	for (i = 0; i < count; i++) {
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

static ssize_t chrdev_read(struct file *file,char __user *buf, size_t size, loff_t *off)
{
    chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    if (copy_from_user(chrdev_private_data->kbuf, buf, 1) != 0) {
        printk("copy_from_user error\r\n");
        return -1;
    }
    chrdev_private_data->kbuf[1] = gpio_get_value(chrdev_private_data->gpios[(int)chrdev_private_data->kbuf[0]].gpio);
    if (copy_to_user(buf, chrdev_private_data->kbuf, 2) != 0) {
        printk("copy_to_user error\r\n");
        return -1;
    }
    printk("This is cdev_test_read\r\n");
	return 0;
}

static ssize_t chrdev_write(struct file *file,const char __user *buf,size_t size,loff_t *off)
{
    chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    if (copy_from_user(chrdev_private_data->kbuf, buf, size) != 0) {
        printk("copy_from_user error\r\n");
        return -1;
    }
    gpio_set_value(chrdev_private_data->gpios[(int)chrdev_private_data->kbuf[0]].gpio, chrdev_private_data->kbuf[1]);

	return 0;
}

static int chrdev_release(struct inode *inode, struct file *file)
{
    int i;
    int count = sizeof(s_char_drv)/sizeof(s_char_drv.gpios[0]);
    chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    for (i = 0; i < count; i++) {
		gpio_free(chrdev_private_data->gpios[i].gpio);		
	}
	return 0;
}

static struct file_operations cdev_test_ops = {
	.owner = THIS_MODULE,
	.open = chrdev_open,
	.read = chrdev_read,
	.write = chrdev_write,
	.release = chrdev_release,
};

static int __init chr_drv_init(void)
{
	int ret;
	/*< 1. Request equipment number */
	if (major) { /*< Static Device ID Setting */
    	s_char_drv.dev_num = MKDEV(major,minor);
    	printk("major is %d\n",major);
    	printk("minor is %d\n",minor);
    	ret = register_chrdev_region(s_char_drv.dev_num,1,"chrdev_name");
        if (ret < 0) {
            printk("register_chrdev_region is error\n");
            goto err_chrdev;
        }
        printk("register_chrdev_region is ok\n");
    } else { /*< Dynamic Device ID Setting */
        ret = alloc_chrdev_region(&s_char_drv.dev_num,0,1,"chrdev_num");
        if (ret < 0) {
            printk("alloc_chrdev_region is error\n");
            goto err_chrdev;
        }
        major=MAJOR(s_char_drv.dev_num);
        minor=MINOR(s_char_drv.dev_num);
        printk("major is %d\n",major);
        printk("minor is %d\n",minor);
    }
	/*< 2. Registered character device */
    cdev_init(&s_char_drv.chr_cdev,&cdev_test_ops);
	s_char_drv.chr_cdev.owner = THIS_MODULE;
    ret = cdev_add(&s_char_drv.chr_cdev,s_char_drv.dev_num,1);
    if(ret < 0 ){
        printk("cdev_add is error\n");
        goto  err_chr_add;
    }
    printk("cdev_add is ok\n");
	/*< 3. Creating a Device Node */
    s_char_drv.chr_class = class_create(THIS_MODULE,"chr_class");
    if (IS_ERR(s_char_drv.chr_class)) {
        printk("class_create is error\n");
        ret = PTR_ERR(s_char_drv.chr_class);
        goto err_class_create;
    }
    s_char_drv.chr_device = device_create(s_char_drv.chr_class,NULL,s_char_drv.dev_num,NULL,"chr_device");
    if (IS_ERR(s_char_drv.chr_device)) {
        printk("device_create is error\n");
        ret = PTR_ERR(s_char_drv.chr_device);
        goto err_device_create;
    }

    return 0;

err_device_create:
    class_destroy(s_char_drv.chr_class);
err_class_create:
    cdev_del(&s_char_drv.chr_cdev);
err_chr_add:
    unregister_chrdev_region(s_char_drv.dev_num, 1);
err_chrdev:
    return ret;
}

static void __exit chr_drv_exit(void)
{
    device_destroy(s_char_drv.chr_class,s_char_drv.dev_num);
    class_destroy(s_char_drv.chr_class);
    cdev_del(&s_char_drv.chr_cdev);
    unregister_chrdev_region(s_char_drv.dev_num,1);

    printk("module exit \n");
}

module_init(chr_drv_init);
module_exit(chr_drv_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CSDN:qq_63922192");
MODULE_DESCRIPTION("LED driver");