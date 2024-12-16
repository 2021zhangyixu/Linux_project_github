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

#include "linux/of.h"
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


#define KBUF_MAX_SIZE      5
#define GPIO_NAME_MAX_LEN  10

struct gpio_desc {
	int gpio;
    char name[GPIO_NAME_MAX_LEN];
};

struct irq_resource {
    unsigned int irq;
    struct irq_data * irq_data;
    u32 irq_type;
};

typedef struct chr_drv {
	int major;                 /*< Master device number */
	int minor;                 /*< Secondary device number */
	dev_t dev_num;             /*< Device number of type dev_t consisting of primary device number and secondary device number */
	struct cdev chr_cdev;      /*< Character device structure */
	struct class *chr_class;   /*< Device class */
    struct device *chr_device; /*< Device instance */
    u64 kbuf;  /*< Kernel buffer */
    struct gpio_desc *gpios;   /*< GPIO pin descriptor */
    struct irq_resource irq_res;
    struct fasync_struct *fasync;
}chr_drv;

static chr_drv s_char_drv;


static irqreturn_t echo_isr_fun(int irq, void *dev_id)
{
    int val;
    static u64 rising_start_ns = 0;
    val = gpio_get_value(s_char_drv.gpios[1].gpio);
    if (val) {
        rising_start_ns = ktime_get_ns(); // s - ms - us - ns
    } else {
        if (rising_start_ns == 0) {
			printk("missing rising interrupt\n");
			return IRQ_HANDLED;
		}
        s_char_drv.kbuf = ktime_get_ns() - rising_start_ns;
        printk("rising time : %llu ns\n",s_char_drv.kbuf);
        kill_fasync(&s_char_drv.fasync,SIGIO,POLL_IN);

        /*< Clear state */
        rising_start_ns = 0;
    }
    
    return IRQ_HANDLED;
}

static int chrdev_open(struct inode *inode, struct file *file)
{
    int ret;
    file->private_data = &s_char_drv;
    /*< trig */
    ret = gpio_request(s_char_drv.gpios[0].gpio, s_char_drv.gpios[0].name);
    if (ret < 0) {
        printk("can not request gpio %s %d\n", s_char_drv.gpios[0].name, s_char_drv.gpios[0].gpio);
        return -ENODEV;
    }
    gpio_direction_output(s_char_drv.gpios[0].gpio, 1);
    /*< echo */
    ret = request_irq(s_char_drv.irq_res.irq,echo_isr_fun,s_char_drv.irq_res.irq_type,"echo_isr",NULL);
    if (ret < 0) {
        printk("request_irq is error\n");
        return ret;
    }
    disable_irq(s_char_drv.irq_res.irq);
	printk("%s line %d\n", __FUNCTION__, __LINE__);
	return 0;
}

static int chrdev_release(struct inode *inode, struct file *file)
{
    chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    /*< trig */
    gpio_free(chrdev_private_data->gpios[0].gpio);
    /*< echo */
    free_irq(chrdev_private_data->irq_res.irq,NULL);
	printk("%s line %d\n", __FUNCTION__, __LINE__);
	return 0;
}

static ssize_t chrdev_write(struct file *file,const char __user *buf,size_t size,loff_t *off)
{
    chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    gpio_set_value(chrdev_private_data->gpios[0].gpio, 1);
    udelay(15);
    gpio_set_value(chrdev_private_data->gpios[0].gpio, 0);
    enable_irq(s_char_drv.irq_res.irq);
    printk("chrdev_write\n");
	return 0;
}

static ssize_t chrdev_read(struct file *file,char __user *buf, size_t size, loff_t *off)
{
    chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    if (copy_to_user(buf, &chrdev_private_data->kbuf, size) != 0) {
        printk("copy_to_user error\r\n");
        return -1;
    }
	return sizeof(u64);
}

static int chrdev_fasync(int fd, struct file *file, int on)
{
    chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    return fasync_helper(fd,file,on,&chrdev_private_data->fasync);
}

static struct file_operations chr_file_operations = {
	.owner = THIS_MODULE,
	.open = chrdev_open,
	.release = chrdev_release,
	.write = chrdev_write,
	.read = chrdev_read,
    .fasync = chrdev_fasync, 
};

static int sr04_drver_probe(struct platform_device *pdev)
{
	int ret;
    struct device_node *echo_np,*trig_np;
    struct device_node *np = pdev->dev.of_node;

	/*< 1. Request equipment number */
    ret = alloc_chrdev_region(&s_char_drv.dev_num,0,1,"chrdev_num");
    if (ret < 0) {
        printk("alloc_chrdev_region is error\n");
        goto err_chrdev;
    }
    printk("1. major is %d ; minor is %d\n",MAJOR(s_char_drv.dev_num),MINOR(s_char_drv.dev_num));
	/*< 2. Registered character device */
    cdev_init(&s_char_drv.chr_cdev,&chr_file_operations);
	s_char_drv.chr_cdev.owner = THIS_MODULE;
    ret = cdev_add(&s_char_drv.chr_cdev,s_char_drv.dev_num,1);
    if(ret < 0 ){
        printk("cdev_add is error\n");
        goto  err_chr_add;
    }
    printk("2. Registered character device is ok\n");
	/*< 3. Creating a Device Node */
    s_char_drv.chr_class = class_create(THIS_MODULE,"chr_class_sr04");
    if (IS_ERR(s_char_drv.chr_class)) {
        ret = PTR_ERR(s_char_drv.chr_device);
        printk("class_create failed with error code: %d\n", ret);
        goto err_class_create;
    }

    s_char_drv.chr_device = device_create(s_char_drv.chr_class,NULL,s_char_drv.dev_num,NULL,"chr_device_sr04");
    if (IS_ERR(s_char_drv.chr_device)) {
        ret = PTR_ERR(s_char_drv.chr_device);
        printk("device_create failed with error code: %d\n", ret);
        goto err_device_create;
    }
    printk("3. creating a device node is ok\n");
    /*< 4. Obtaining device resources */
    s_char_drv.gpios = kmalloc((2 * sizeof(struct gpio_desc)), GFP_KERNEL);
    if (s_char_drv.gpios == NULL) {
        printk("Device tree: kmalloc is flaut\r\n");
        ret = -EINVAL;
        goto err_get_resource;
    }
    /*< trig */
    trig_np = of_find_node_by_name(np, "trig");
    s_char_drv.gpios[0].gpio = of_get_gpio(trig_np, 0);
    ret = sprintf(s_char_drv.gpios[0].name, "%s", trig_np->name);
    if (ret < 0) {
        printk("sprintf is flaut \n");
        goto err_get_resource;
    }
    printk("gpios[0].gpio is %d name is %s\n",s_char_drv.gpios[0].gpio,s_char_drv.gpios[0].name);
    /*< echo */
    echo_np = of_find_node_by_name(np, "echo");
    s_char_drv.gpios[1].gpio = of_get_gpio(echo_np, 0);
    ret = sprintf(s_char_drv.gpios[1].name, "%s", echo_np->name);
    if (ret < 0) {
        printk("sprintf is flaut \n");
        goto err_get_resource;
    }
    printk("gpios[1].gpio is %d name is %s\n",s_char_drv.gpios[1].gpio,s_char_drv.gpios[1].name);
    s_char_drv.irq_res.irq = irq_of_parse_and_map(echo_np,0);
    if (s_char_drv.irq_res.irq == 0) {
        printk("irq_of_parse_and_map error\n");
        goto err_get_resource;
    }
    s_char_drv.irq_res.irq_data = irq_get_irq_data(s_char_drv.irq_res.irq);
    if (s_char_drv.irq_res.irq_data == NULL) {
        printk("irq_get_irq_data error\n");
        goto err_get_resource;
    }
    s_char_drv.irq_res.irq_type = irqd_get_trigger_type(s_char_drv.irq_res.irq_data);
    printk("4. Obtaining device resources is ok\n");

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

static int sr04_drver_remove(struct platform_device *pdev)
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
	{.compatible = "sr04@1"},
    { /* Sentinel (end of array marker) */ }
};

static struct platform_driver sr04_driver = {
	.driver		= {
		.owner = THIS_MODULE,
        .name  = "sr04_driver",
		.of_match_table = dts_table,
	},
	.probe		= sr04_drver_probe,
	.remove		= sr04_drver_remove,
};

static int __init chr_drv_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&sr04_driver);
	if(ret < 0) {
		printk("platform_driver_register error \n");
		return ret;
	}
    return 0;
}

static void __exit chr_drv_exit(void)
{
	platform_driver_unregister(&sr04_driver);
	printk("platform_driver_unregister ok \n");
}

module_init(chr_drv_init);
module_exit(chr_drv_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CSDN:qq_63922192");
MODULE_DESCRIPTION("LED driver");