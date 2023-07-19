/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释。
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是应用层代码
 * 作者 ： CSDN风正豪
*/

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

struct gpio_desc{
	int gpio;
	int irq;
	char name[128];
	int key;
	struct timer_list key_timer;
} ;

static struct gpio_desc *gpios; //描述gpio
static int count;

/* 主设备号                                                                 */
static int major = 0;
static struct class *gpio_class;

/* 马达引脚设置数字 */
static int g_motor_pin_ctrl[8]= {0x2,0x3,0x1,0x9,0x8,0xc,0x4,0x6};
static int g_motor_index = 0;

void set_pins_for_motor(int index)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		gpio_set_value(gpios[i].gpio, g_motor_pin_ctrl[index] & (1<<i) ? 1 : 0);
	}
}

void disable_motor(void)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		gpio_set_value(gpios[i].gpio, 0);
	}
}

/* int buf[2];
 * buf[0] = 步进的次数, > 0 : 逆时针步进; < 0 : 顺时针步进
 * buf[1] = mdelay的时间
 */
static ssize_t motor_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    int ker_buf[2];
    int err;
	int step;


    if (size != 8)
        return -EINVAL;

    err = copy_from_user(ker_buf, buf, size);


	if (ker_buf[0] > 0)
	{
		/* 逆时针旋转 */
		for (step = 0; step < ker_buf[0]; step++)
{
			set_pins_for_motor(g_motor_index);
			mdelay(ker_buf[1]);
			g_motor_index--;
			if (g_motor_index == -1)
				g_motor_index = 7;

}
	}
	else
	{
		/* 顺时针旋转 */
		ker_buf[0] = 0 - ker_buf[0];
		for (step = 0; step < ker_buf[0]; step++)
{
			set_pins_for_motor(g_motor_index);
			mdelay(ker_buf[1]);
			g_motor_index++;
			if (g_motor_index == 8)
				g_motor_index = 0;

		}
}

	/* 改进：旋转到位后让马达不再消耗电源 */
	disable_motor();

    return 8;    
}



/* 定义自己的file_operations结构体                                              */
static struct file_operations gpio_key_drv = {
	.owner	 = THIS_MODULE,
	.write   = motor_write,
};

/* 在入口函数 */
static int gpio_drv_probe(struct platform_device *pdev)
{
    int err = 0;
    int i;
	struct device_node *np = pdev->dev.of_node;  //在这个平台设备来自于设备树，那么np就不是空的
	struct resource *res;
	
	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	

	/* 从platfrom_device获得引脚信息 
	 * 1. pdev来自c文件
   * 2. pdev来自设备树
	 */
	
	if (np)
	{
		/* pdev来自设备树 : 示例
        reg_usb_ltemodule: regulator@1 {
            compatible = "100ask,gpiodemo";
                   //GPIO 控制器      引脚号      极性（可选）
            gpios = <&gpio5        5      GPIO_ACTIVE_HIGH>, <&gpio5 3 GPIO_ACTIVE_HIGH>;
        };
		*/
		/* 作用 ： 获取设备树中某个设备节点所定义的 GPIO（通用输入输出）的数量
		 * np ：指向要查询的设备节点（struct device_node 结构体）的指针
		 * 返回值 ： 成功返回设备节点中定义的 GPIO 的数量，失败就返回负数
		*/
		count = of_gpio_count(np); //统计的设备的 GPIO 数量
		if (!count)
			return -EINVAL;
		
		/* 作用 ： Linux 内核中用于动态分配内核空间的函数之一
		 * size ：要分配的内存区域的大小（以字节为单位）
		 * flags ：分配标志，用于指定内存分配的行为和属性，例如内存的连续性、是否可以等待等，一般选择GFP_KERNEL
		 * 返回值 ： 成功返回指向分配的内存区域的指针，失败就返回NULL
		*/
		gpios = kmalloc(count * sizeof(struct gpio_desc), GFP_KERNEL);   //为gpio信息申请内存
		for (i = 0; i < count; i++)
		{		
		/* 作用 ： 获取设备树中某个设备节点所定义的 GPIO（通用输入输出）的引脚号
		 * np ：指向要查询的设备节点（struct device_node 结构体）的指针
		 * index ：GPIO 的索引号，用于指定要获取的 GPIO 在设备节点 np 中的位置
		 * 返回值 ： 成功返回指定 GPIO 的引脚号，失败就返回负数
		*/
			gpios[i].gpio = of_get_gpio(np, i);  //从节点里面的gpios中取出第i项
			sprintf(gpios[i].name, "%s_pin_%d", np->name, i); //按照上面的例子，gpios[i].name == gpio5_pin_5
		}
	}
	else
	{
		/* pdev来自c文件 
		static struct resource omap16xx_gpio3_resources[] = {
			{
					.start  = 115,
					.end    = 115,
					.flags  = IORESOURCE_IRQ,
			},
			{
					.start  = 118,
					.end    = 118,
					.flags  = IORESOURCE_IRQ,
			},		};		
		*/
		count = 0;
		while (1)
		{
			/* 下面这9行，是用于统计有多少个GPIO的
			 * dev：一个指向 platform_device 结构的指针，表示要获取资源信息的设备。
			 * type：一个无符号整数，表示要获取的资源类型。在 Linux 内核中，资源类型使用常量来表示，
			        例如 IORESOURCE_MEM 表示内存资源，IORESOURCE_IRQ 表示中断资源等。你可以根据需要选择适当的资源类型。
			 * num：一个无符号整数，表示要获取的资源的索引号。在一个设备中可能存在多个相同类型的资源，通过索引号可以区分它们。
			 * 返回值：返回一个指向 resource 结构的指针，表示获取到的资源信息。
			           resource 结构包含了资源的起始地址、大小等信息。如果没有找到指定的资源，函数将返回 NULL。
			*/
			res = platform_get_resource(pdev, IORESOURCE_IRQ, count);
			if (res)
			{
				count++;
			}
			else
			{
				break;
			}
		}

		if (!count)
			return -EINVAL;

		//GFP_KERNEL是内存分配的标志之一，它表示在内核中以普通的内核上下文进行内存分配
		gpios = kmalloc(count * sizeof(struct gpio_desc), GFP_KERNEL); 
		for (i = 0; i < count; i++)
		{
			res = platform_get_resource(pdev, IORESOURCE_IRQ, i);  //从节点里面取出第i项
			gpios[i].gpio = res->start;                            //将需要操作的IO号传递给gpios[i].gpio
			sprintf(gpios[i].name, "%s_pin_%d", pdev->name, i);   //按照上面的例子，gpios[i].name == gpio5_pin_5
		}

	}

	for (i = 0; i < count; i++)
	{
		//注册IO
		err = gpio_request(gpios[i].gpio, gpios[i].name);
		//将IO设置为输出，默认低电平
		gpio_direction_output(gpios[i].gpio, 0);
	}

	/* 注册file_operations 	*/
	major = register_chrdev(0, "100ask_gpio_key", &gpio_key_drv);  /* /dev/gpio_desc */

	/******这里相当于命令行输入 mknod     /dev/sr04 c 240 0 创建设备节点*****/
	
	//创建类，为THIS_MODULE模块创建一个类，这个类叫做100ask_gpio_key_class
	gpio_class = class_create(THIS_MODULE, "100ask_gpio_key_class");
	//如果类创建失败
	if (IS_ERR(gpio_class))
	{
		/*__FILE__ ：表示文件
		 *__FUNCTION__ ：当前函数名
		 *__LINE__ ：在文件的哪一行
		*/
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		//注销驱动
		unregister_chrdev(major, "100ask_gpio_key");
		//返回错误
		return PTR_ERR(gpio_class);
	}

	/*输入参数是逻辑设备的设备名，即在目录/dev目录下创建的设备名
	 *参数一 ： 在gpio_class类下面创建设备
	 *参数二 ： 无父设备的指针
	 *参数三 ： dev为主设备号+次设备号
	 *参数四 ： 没有私有数据
	*/
	device_create(gpio_class, NULL, MKDEV(major, 0), NULL, "motor"); /* /dev/motor */
	
	return err;
}

/* 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 */
static int gpio_drv_remove(struct platform_device *pdev)
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
	unregister_chrdev(major, "100ask_gpio_key");

	for (i = 0; i < count; i++)
	{
		//释放IO口
		gpio_free(gpios[i].gpio);
	}

	return 0;
}

static const struct of_device_id gpio_dt_ids[] = {
        { .compatible = "100ask,gpiodemo", },  //表示可以支持来自于设备树的某一些平台
        { /* sentinel */ }
};

static struct platform_driver gpio_platform_driver = {
	.driver		= {
		.name	= "100ask_gpio_plat_drv",   //根据这个名字，找到设备
		.of_match_table = gpio_dt_ids,
	},
	.probe		= gpio_drv_probe,   //注册平台之后，内核如果发现支持某一个平台设备，这个函数就会被调用。入口函数
	.remove		= gpio_drv_remove,  //出口函数
};

//注册时候的入口函数
static int __init gpio_drv_init(void)
{
	/* 注册platform_driver */
	return platform_driver_register(&gpio_platform_driver);  //注意，这里是driver表示是驱动
}

//卸载时候的出口函数
static void __exit gpio_drv_exit(void)
{
	/* 反注册platform_driver */
	platform_driver_unregister(&gpio_platform_driver);   //注意，这里是driver表示是驱动
}

/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(gpio_drv_init);  //确认入口函数
module_exit(gpio_drv_exit);  //确认出口函数

/*最后我们需要在驱动中加入 LICENSE 信息和作者信息，其中 LICENSE 是必须添加的，否则的话编译的时候会报错，作者信息可以添加也可以不添加
 *这个协议要求我们代码必须免费开源，Linux遵循GPL协议，他的源代码可以开放使用，那么你写的内核驱动程序也要遵循GPL协议才能使用内核函数
 *因为指定了这个协议，你的代码也需要开放给别人免费使用，同时可以根据这个协议要求很多厂商提供源代码
 *但是很多厂商为了规避这个协议，驱动源代码很简单，复杂的东西放在应用层
*/
MODULE_LICENSE("GPL"); //指定模块为GPL协议
MODULE_AUTHOR("CSDN:qq_63922192");  //表明作者，可以不写



