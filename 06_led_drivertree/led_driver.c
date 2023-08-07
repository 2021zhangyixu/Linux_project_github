/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释。
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是驱动层代码
 	*4，TAB为4个空格
 * 作者 ： CSDN风正豪
*/

#include "asm-generic/errno-base.h"
#include "asm-generic/gpio.h"
#include "asm/uaccess.h"
#include <linux/module.h>
#include <linux/poll.h>

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

//描述一个引脚
struct gpio_desc{
	int gpio;   //引脚编号
    char name[128]; //名字
};

static struct gpio_desc *gpios;   //描述gpio
static int count;                 //记录有多少个GPIO

/* 主设备号                                                                 */
static int major = 0;
static struct class *gpio_class;  //一个类，用于创建设备节点


/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t gpio_drv_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	char tmp_buf[2];  //存放驱动层和应用层交互的信息
	int ret;   //没有使用，用于存放copy_from_user和copy_to_user的返回值，消除报错

	//应用程序读的时候，传入的值如果不是两个，那么返回一个错误
	if (size != 2)
		return -EINVAL;
	
	/* 作用 ： 驱动层得到应用层数据
	 * tmp_buf : 驱动层数据
	 * buf ： 应用层数据
	 * 1  ：数据长度为1个字节（因为我只需要知道他控制的是那一盏灯，所以只需要传入一个字节数据）
	 * 返回值 : 失败返回没有被拷贝的字节数，成功返回0.
	*/
	ret = copy_from_user(tmp_buf, buf, 1);
	if(ret != 0)
	{
		printk("copy_from_user is error\r\n");
		return ret;
	}
	
	//第0项表示要操作哪一个LED，如果操作的LED超出，表示失败
	if (tmp_buf[0] >= count)
		return -EINVAL;
	
	//将引脚电平读取出来
	tmp_buf[1] = gpio_get_value(gpios[(int)tmp_buf[0]].gpio);
	
	/* 作用 ： 驱动层发数据给应用层
	 * buf ： 应用层数据
	 * tmp_buf : 驱动层数据
	 * 2  ：数据长度为2个字节
	 * 返回值 ： 成功返回0，失败返回没有拷贝成功的数据字节数
	*/
	ret = copy_to_user(buf, tmp_buf, 2);
	if(ret != 0)
	{
		printk("copy_to_user is error\r\n");
		return ret;
	}
	
	return 2;
}

static ssize_t gpio_drv_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    unsigned char ker_buf[2];
    int ret;
	//应用程序读的时候，传入的值如果不是两个，那么返回一个错误
    if (size != 2)
	{
        return -EINVAL;
	}

	/* 作用 ： 驱动层得到应用层数据
	 * tmp_buf : 驱动层数据
	 * buf ： 应用层数据
	 * size  ：数据长度为size个字节
	 * 返回值 : 失败返回没有被拷贝的字节数，成功返回0.
	*/
    ret = copy_from_user(ker_buf, buf, size);
	if(ret != 0)
	{
		printk("copy_from_user is error\r\n");
		return ret;
	}


	//如果要操作的GPIO不在规定范围内，返回错误
    if (ker_buf[0] >= count)
        return -EINVAL;

	//设置指定引脚电平
    gpio_set_value(gpios[ker_buf[0]].gpio, ker_buf[1]);
    return 2;    
}



/* 定义自己的file_operations结构体                                              */
static struct file_operations gpio_led_drv = {
	.owner	 = THIS_MODULE,
	.read    = gpio_drv_read,
	.write   = gpio_drv_write,
};


/* 在入口函数 */
static int led_drver_probe(struct platform_device *pdev)
{
	int err = 0;  //用于保存函数返回值，用于判断函数是否执行成功
	int i;    //因为存在多个GPIO可能要申请，所以建立一个i进行for循环
	struct device_node *np = pdev->dev.of_node;
	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	if (np)
	{
		/* pdev来自设备树 : 示例
        reg_usb_ltemodule: regulator@1 {
            compatible = "100ask,gpiodemo";
            gpios = <&gpio5 5 GPIO_ACTIVE_HIGH>, <&gpio5 3 GPIO_ACTIVE_HIGH>;
        };
		*/
		count = of_gpio_count(np); //统计的设备的 GPIO 数量
		if (!count)
			return -EINVAL;
		
		printk("count is %d\r\n",count);
		/* 作用 ：  kmalloc是Linux内核中的一个内存分配函数，用于在内核空间中动态分配内存。
		 *				它类似于C语言中的malloc函数，但是在内核中使用kmalloc而不是 malloc，因为内核空间和用户空间有不同的内存管理机制。
		 * size ： 要分配的内存大小，以字节为单位。
		 * flags ： 分配内存时的标志，表示内存的类型和分配策略，是一个 gfp_t 类型的值。常常采用GFP_KERNEL
		 *					GFP_KERNEL是内存分配的标志之一，它表示在内核中以普通的内核上下文进行内存分配。
		 * 返回值 ： 如果内存分配成功，返回指向分配内存区域的指针。如果内存分配失败（例如内存不足），返回NULL。
		*/

		gpios = kmalloc(count * sizeof(struct gpio_desc), GFP_KERNEL);
		if(gpios == NULL)
		{
			printk("kmalloc is flaut\r\n");
		}
		printk("kmalloc is ok\r\n");
		
		for (i = 0; i < count; i++)
		{
			/* 设置为输出引脚 */
			gpios[i].gpio = of_get_gpio(np, i);   //获得gpio信息
			sprintf(gpios[i].name, "%s_pin_%d", np->name, i);  //将platform_device.resource.name传递给gpios[i].name
			printk("gpios[%d] is ok\r\n",i);
			//申请指定GPIO引脚，申请的时候需要用到名字
			err = gpio_request(gpios[i].gpio, gpios[i].name);
			//如果返回值小于0，表示申请失败
			if (err < 0) 
			{
				//如果GPIO申请失败，打印出是哪个LED申请出现问题
				printk("can not request gpio %s \n", gpios[i].name);
				return -ENODEV;
			}
			//如果GPIO申请成功，设置输出高电平
			gpio_direction_output(gpios[i].gpio, 1);
			printk("gpios[%d] is setting\r\n",i);	
		}
	}
	
	printk("GPIO_set is ok\r\n");

	/* 注册file_operations 	*/
	//注册字符驱动程序
	major = register_chrdev(0, "100ask_led", &gpio_led_drv);  /* /dev/gpio_desc */
	
	/******这里相当于命令行输入 mknod  /dev/100ask_gpio c 240 0 创建设备节点*****/
	
	//创建类，为THIS_MODULE模块创建一个类，这个类叫做gpio_class
	gpio_class = class_create(THIS_MODULE, "100ask_led_class");
	if (IS_ERR(gpio_class))   //如果返回错误
	{
		/*__FILE__ ：表示文件
		 *__FUNCTION__ ：当前函数名
		 *__LINE__ ：在文件的哪一行
		*/
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		//注销字符驱动程序
		unregister_chrdev(major, "100ask_led_class");
		//返回错误
		return PTR_ERR(gpio_class);
	}
	
	/*输入参数是逻辑设备的设备名，即在目录/dev目录下创建的设备名
	 *参数一 ： 在gpio_class类下面创建设备
	 *参数二 ： 无父设备的指针
	 *参数三 ： 主设备号+次设备号
	 *参数四 ： 没有私有数据
	*/
	device_create(gpio_class, NULL, MKDEV(major, 0), NULL, "100ask_led"); /* /dev/100ask_gpio */

	//如果执行到这里了，说明LED驱动装载完成
	printk("LED driver loading is complete\n");
	return err;
}

//static int led_drver_probe(struct platform_device *pdev)
//{
//	//打印匹配到设备名字
//	printk("This is %s,gpio is %d\r\n",pdev->resource[0].name,pdev->resource[0].start);
//	printk("This is %s,gpio is %d\r\n",pdev->resource[1].name,pdev->resource[1].start);
//	return 0;
//}

/* 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 */
static int led_drver_remove(struct platform_device *pdev)
{
    int i;
	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	//销毁gpio_class类下面的设备节点
	device_destroy(gpio_class, MKDEV(major, 0));
	//销毁gpio_class类
	class_destroy(gpio_class);
	//注销驱动
	unregister_chrdev(major, "100ask_led");

	for (i = 0; i < count; i++)
	{
		//将GPIO释放
		gpio_free(gpios[i].gpio);		
	}
	
	//如果执行到这里了，说明LED驱动卸载完成
	printk("The LED driver is uninstalled\n");
	return 0;
}


static const struct of_device_id led_dtb[] = 
{
	{.compatible = "test_compatible", }, //要求和设备树的compatible名字一样
	{ /* sentinel */ }
};


static struct platform_driver led_driver = {
	.driver		= {
		.owner = THIS_MODULE,
		.name	= "led_device",   //根据这个名字，找到设备
		.of_match_table = led_dtb, //与设备树有关
	},
	.probe		= led_drver_probe,   //注册平台之后，内核如果发现支持某一个平台设备，这个函数就会被调用。入口函数
	.remove		= led_drver_remove,  //出口函数
};


//注册时候的入口函数
static int __init led_drver_init(void)
{
	int ret = 0;
	// platform驱动注册到 Linux 内核
	ret = platform_driver_register(&led_driver);  //注意，这里是driver表示是驱动
	if(ret<0)
	{
		printk("platform_driver_register error \n");
		return ret;
	}
	printk("platform_driver_register ok \n");
	return ret;
}

//卸载时候的出口函数
static void __exit led_drver_exit(void)
{
	/* 反注册platform_driver */
	platform_driver_unregister(&led_driver);   //注意，这里是driver表示是驱动
	printk("platform_driver_unregister ok \n");
}


module_init(led_drver_init);  //确认入口函数
module_exit(led_drver_exit);  //确认出口函数

/*最后我们需要在驱动中加入 LICENSE 信息和作者信息，其中 LICENSE 是必须添加的，否则的话编译的时候会报错，作者信息可以添加也可以不添加
 *这个协议要求我们代码必须免费开源，Linux遵循GPL协议，他的源代码可以开放使用，那么你写的内核驱动程序也要遵循GPL协议才能使用内核函数
 *因为指定了这个协议，你的代码也需要开放给别人免费使用，同时可以根据这个协议要求很多厂商提供源代码
 *但是很多厂商为了规避这个协议，驱动源代码很简单，复杂的东西放在应用层
*/
MODULE_LICENSE("GPL"); //指定模块为GPL协议
MODULE_AUTHOR("CSDN:qq_63922192");  //表明作者，可以不写



