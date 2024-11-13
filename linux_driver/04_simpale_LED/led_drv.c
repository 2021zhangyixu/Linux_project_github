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

static struct gpio_desc gpios[] = {
    {131, "led0" },  //引脚编号，名字
};

/* 主设备号                                                                 */
static int major = 0;
static struct class *gpio_class;  //一个类，用于创建设备节点


/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t gpio_drv_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	char tmp_buf[2];  //存放驱动层和应用层交互的信息
	int ret;   //没有使用，用于存放copy_from_user和copy_to_user的返回值，消除报错
	int count = sizeof(gpios)/sizeof(gpios[0]); //记录定义的最大引脚数量

	//应用程序读的时候，传入的值如果不是两个，那么返回一个错误
	if (size != 2)
		return -EINVAL;
	
	/* 作用 ： 驱动层得到应用层数据
	 * tmp_buf : 驱动层数据
	 * buf ： 应用层数据
	 * 1  ：数据长度为1个字节（因为我只需要知道他控制的是那一盏灯，所以只需要传入一个字节数据）
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
    if (ker_buf[0] >= sizeof(gpios)/sizeof(gpios[0]))
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
static int __init gpio_drv_init(void)
{
    int err = 0;  //用于保存函数返回值，用于判断函数是否执行成功
    int i;    //因为存在多个GPIO可能要申请，所以建立一个i进行for循环
    int count = sizeof(gpios)/sizeof(gpios[0]);  //统计有多少个GPIO
	
	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	for (i = 0; i < count; i++)
	{		
		/* 设置为输出引脚 */
		//申请指定GPIO引脚，申请的时候需要用到名字
		err = gpio_request(gpios[i].gpio, gpios[i].name);
		//如果返回值小于0，表示申请失败
		if (err < 0) 
		{
			//如果GPIO申请失败，打印出是哪个GPIO申请出现问题
			printk("can not request gpio %s %d\n", gpios[i].name, gpios[i].gpio);
			return -ENODEV;
		}
		//如果GPIO申请成功，设置输出高电平
		gpio_direction_output(gpios[i].gpio, 1);
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

/* 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 */
static void __exit gpio_drv_exit(void)
{
    int i;
    int count = sizeof(gpios)/sizeof(gpios[0]);
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
}


/* 7. 其他完善：提供设备信息                  */

module_init(gpio_drv_init);  //确认入口函数
module_exit(gpio_drv_exit);  //确认出口函数

/*最后我们需要在驱动中加入 LICENSE 信息和作者信息，其中 LICENSE 是必须添加的，否则的话编译的时候会报错，作者信息可以添加也可以不添加
 *这个协议要求我们代码必须免费开源，Linux遵循GPL协议，他的源代码可以开放使用，那么你写的内核驱动程序也要遵循GPL协议才能使用内核函数
 *因为指定了这个协议，你的代码也需要开放给别人免费使用，同时可以根据这个协议要求很多厂商提供源代码
 *但是很多厂商为了规避这个协议，驱动源代码很简单，复杂的东西放在应用层
*/
MODULE_LICENSE("GPL"); //指定模块为GPL协议
MODULE_AUTHOR("CSDN:qq_63922192");  //表明作者，可以不写



