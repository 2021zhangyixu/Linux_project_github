/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释。
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是应用层代码
 * 作者 ： CSDN风正豪
*/


#include "asm-generic/gpio.h"
#include "asm/gpio.h"
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

//描述一个引脚
struct gpio_desc{
	int gpio;   //引脚编号
	int irq;    //中断号
    char *name; //名字
    int key;    //按键值
	struct timer_list key_timer; //定时器，用于消除抖动
} ;


static struct gpio_desc gpios[] = {
    {115, 0, "motor_gpio0", },  //编号，引脚电平，名字
    {116, 0, "motor_gpio1", },
    {117, 0, "motor_gpio2", },
    {118, 0, "motor_gpio3", },
};

/* 主设备号                                                                 */
static int major = 0;
static struct class *gpio_class;    //一个类，用于创建设备节点

/* 马达引脚设置数字 */
static int g_motor_pin_ctrl[8]= {0x2,0x3,0x1,0x9,0x8,0xc,0x4,0x6};  //顺时针旋转
static int g_motor_index = 0;

void set_pins_for_motor(int index)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		gpio_set_value(gpios[i].gpio, g_motor_pin_ctrl[index] & (1<<i) ? 1 : 0);
	}
}

//旋转完成之后，让电机处于高阻态
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
 * buf[1] = 每步进一次要延时多长时间
 */
static ssize_t motor_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    int ker_buf[2];
    int err;
	int step;

	//用户空间拷贝到内核空间的数据大小为8个字节
    if (size != 8)
        return -EINVAL;
	
	/* 作用 ： 驱动层得到应用层数据
	 * tmp_buf : 驱动层数据
	 * buf ： 应用层数据
	 * size  ：数据长度为size个字节
	*/
    err = copy_from_user(ker_buf, buf, size);
    
    //判断是顺时针旋转还是逆时针旋转
	if (ker_buf[0] > 0)
	{
		/* 逆时针旋转 */
		for (step = 0; step < ker_buf[0]; step++)
		{
			set_pins_for_motor(g_motor_index);
			mdelay(ker_buf[1]); //延时，单位ms
			g_motor_index--;
			if (g_motor_index == -1)  //进行圆形循环
				g_motor_index = 7;

		}
	}
	else
	{
		/* 顺时针旋转 */
		ker_buf[0] = 0 - ker_buf[0];   //取绝对值
		for (step = 0; step < ker_buf[0]; step++)
		{
			set_pins_for_motor(g_motor_index);
			mdelay(ker_buf[1]);
			g_motor_index++;
			if (g_motor_index == 8)  //进行圆形循环
				g_motor_index = 0;

		}
	}

	/* 改进：旋转到位后让马达不再消耗电源 */
	disable_motor();

	//返回写入字节数量
    return 8;    
}



/* 定义自己的file_operations结构体                                              */
static struct file_operations gpio_key_drv = {
	.owner	 = THIS_MODULE,
	.write   = motor_write,
};

/* 在入口函数 */
static int __init motor_init(void)
{
    int err;   //用于存储申请GPIO数量
    int i;     //用于for语句
    int count = sizeof(gpios)/sizeof(gpios[0]);  //计算需要注册多少个GPIO
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	
	for (i = 0; i < count; i++)
	{
		//申请GPIO
		err = gpio_request(gpios[i].gpio, gpios[i].name);
		//将GPIO设置为输出，并且启动为低电平
		gpio_direction_output(gpios[i].gpio, 0);
	}

	/* 注册file_operations 	*/
	major = register_chrdev(0, "100ask_gpio_key", &gpio_key_drv);  /* /dev/gpio_desc */

	/******这里相当于命令行输入 mknod 	/dev/sr04 c 240 0 创建设备节点*****/
	
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
		//打印类创建失败
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
static void __exit motor_exit(void)
{
    int i;  //用于for循环
    int count = sizeof(gpios)/sizeof(gpios[0]);   //计算要释放的引脚数量
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

	//释放引脚
	for (i = 0; i < count; i++)
	{
		gpio_free(gpios[i].gpio);
	}
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(motor_init);  //确认入口函数
module_exit(motor_exit);  //确认出口函数

/*最后我们需要在驱动中加入 LICENSE 信息和作者信息，其中 LICENSE 是必须添加的，否则的话编译的时候会报错，作者信息可以添加也可以不添加
 *这个协议要求我们代码必须免费开源，Linux遵循GPL协议，他的源代码可以开放使用，那么你写的内核驱动程序也要遵循GPL协议才能使用内核函数
 *因为指定了这个协议，你的代码也需要开放给别人免费使用，同时可以根据这个协议要求很多厂商提供源代码
 *但是很多厂商为了规避这个协议，驱动源代码很简单，复杂的东西放在应用层
*/
MODULE_LICENSE("GPL"); //指定模块为GPL协议
MODULE_AUTHOR("CSDN:qq_63922192");  //表明作者，可以不写



