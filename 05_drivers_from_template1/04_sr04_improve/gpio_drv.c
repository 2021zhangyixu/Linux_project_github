/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释。
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是应用层代码
 * 作者 ： CSDN风正豪
*/


#include "asm-generic/errno.h"
#include "asm-generic/gpio.h"
#include "asm/delay.h"
#include "linux/jiffies.h"
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

#define CMD_TRIG  100  //表示开始发送超声波，可以为任意值，不建议0或者1，因为已经被占用

//描述一个引脚
struct gpio_desc{
	int gpio;   //引脚编号
	int irq;    //中断号
    char *name; //名字
    int key;    //按键值
	struct timer_list key_timer; //定时器，用于消除抖动
} ;


static struct gpio_desc gpios[2] = {
    {115, 0, "trig", },  //编号，引脚电平，名字
    {116, 0, "echo", }, 
};

/* 主设备号                                                                 */
static int major = 0;
static struct class *gpio_class;   //一个类，用于创建设备节点

/* 环形缓冲区 */
#define BUF_LEN 128
static int g_keys[BUF_LEN];
static int r, w;

struct fasync_struct *button_fasync;    //用于异步通知机制

#define NEXT_POS(x) ((x+1) % BUF_LEN)   //用于环形缓冲区的循环

//判断环形缓冲区是否为空
static int is_key_buf_empty(void)
{
	//读位置等于写位置，表示空
	return (r == w);
}

//判断环形缓冲区是否满了
static int is_key_buf_full(void)
{
	return (r == NEXT_POS(w));
}

//将数据写入环形缓冲区
static void put_key(int key)
{
	//如果环形缓冲区没有满
	if (!is_key_buf_full())
	{
		//把按键值放入环形缓冲区的w位置
		g_keys[w] = key;
		//w指向下一个
		w = NEXT_POS(w);
	}
}

//读取引脚电平
static int get_key(void)
{
	int key = 0;
	//如果环形缓冲区数据不为空
	if (!is_key_buf_empty())
	{
		//从r的位置读取数据
		key = g_keys[r];
		//让r为下一个位置
		r = NEXT_POS(r);
	}
	return key;
}


static DECLARE_WAIT_QUEUE_HEAD(gpio_wait);

/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t sr04_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	//printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	int err;
	int key;
	
	//如果环形缓冲区没有数据，并且打开的文件flag为非阻塞
	if (is_key_buf_empty() && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;
	
	//这个打印会影响到中断，在这个函数里面可能存在关中断
	// printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	/* 如果是阻塞模式，这里将会等待is_key_buf_empty()为真。 
	 * 1，等待过程会改变程序状态
	 * 2，把这个程序记录在gpio_wait（队列）中
	 * 在休眠期间，可以被正常的中断唤醒。也有可能中断丢失，由定时器唤醒。
	*/
	wait_event_interruptible(gpio_wait, !is_key_buf_empty());
	//获得按键值数据，读取环形缓冲区。如果是定时器唤醒，key会是-1
	key = get_key();

	//如果key为-1，就返回一个错误
	if (key == -1)
		return -ENODATA;
	
	/* 作用 ： 驱动层发数据给应用层
	 * buf  ： 应用层数据
	 * &key : 驱动层数据
	 * len  ：数据长度
	*/
	err = copy_to_user(buf, &key, 4);
	//传输字节个数
	return 4;
}

//poll机制
static unsigned int sr04_poll(struct file *fp, poll_table * wait)
{
	//printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	//将当前的进程放到gpio_wait队列中，并没有休眠
	poll_wait(fp, &gpio_wait, wait);
	/* 当应用层调用poll函数的时候，内核就会调用sys_poll函数。而sys_poll又会调用驱动层的sr04_poll
	 * 判断buf里面是否有数据，如果有数据，返回POLLIN，没有数据返回0
	 * 如果返回0，那么在内核的sys_poll函数就会休眠一段时间
	 * 如果返回POLLIN，那么就立刻将数据返回给应用层
	*/
	return is_key_buf_empty() ? 0 : POLLIN | POLLRDNORM;
}

//异步通知机制
static int sr04_fasync(int fd, struct file *file, int on)
{
	if (fasync_helper(fd, file, on, &button_fasync) >= 0)
		return 0;
	else
		return -EIO;
}


// ioctl(fd, CMD, ARG)
static long sr04_ioctl(struct file *filp, unsigned int command, unsigned long arg)
{
	// send trig 
	switch (command)
	{
		case CMD_TRIG:
		{
			//printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
			//让超声波开始运行
			gpio_set_value(gpios[0].gpio, 1);
			udelay(20);
			gpio_set_value(gpios[0].gpio, 0);

			/* mod_timer()相当于del_timer(timer)删除定时器 => timer->expires = expires设置定时器超时时间 => add_timer(timer)
			 * 指定定时器
			 * 修改超时时间为当前时间+50ms
			*/
			mod_timer(&gpios[1].key_timer, jiffies + msecs_to_jiffies(50));  
		}
	}

	return 0;
}

/* 定义自己的file_operations结构体                                              */
static struct file_operations sr04_drv = {
	.owner	 = THIS_MODULE,
	.read    = sr04_read,               //读
	.poll    = sr04_poll,               //poll机制
	.fasync  = sr04_fasync,             //异步通知机制
	.unlocked_ioctl = sr04_ioctl,
};

//中断函数
static irqreturn_t sr04_isr(int irq, void *dev_id)
{
	struct gpio_desc *gpio_desc = dev_id;
	int val;
	static u64 rising_time = 0;   //记录上升沿开始时候的时间
	u64 time;      //记录上升沿持续的时间

	//获得引脚电平
	val = gpio_get_value(gpio_desc->gpio);
	//因为打开内核调试信息之后，由于中断函数中的prink函数太耗时，会导致中断丢失，所以这里注释掉
	//printk("sr04_isr echo pin %d is %d\n", gpio_desc->gpio, val);

	if (val)
	{
		/* 上升沿记录起始时间 */
		rising_time = ktime_get_ns();
	}
	else
	{
		//如果漏掉上升沿中断
		if (rising_time == 0)
		{
			//因为打开内核调试信息之后，由于中断函数中的prink函数太耗时，会导致中断丢失，所以这里注释掉
			//printk("missing rising interrupt\n");
			return IRQ_HANDLED;
		}

		/* 下降沿记录结束时间, 并计算时间差, 计算距离 */
		// 停止定时器
		del_timer(&gpios[1].key_timer);

		time = ktime_get_ns() - rising_time;
		rising_time = 0;
		
		//内核中不能使用除法，所以这里直接传入时间参数
		put_key(time);

		wake_up_interruptible(&gpio_wait);
		kill_fasync(&button_fasync, SIGIO, POLL_IN);
		
	}

	return IRQ_HANDLED;
}

//定时器超时函数
static void sr04_timer_func(unsigned long data)
{
	put_key(-1);
	wake_up_interruptible(&gpio_wait);
	kill_fasync(&button_fasync, SIGIO, POLL_IN);
}

/* 在入口函数 */
static int __init sr04_init(void)
{
    int err;
	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	
	// trig pin
	err = gpio_request(gpios[0].gpio, gpios[0].name);
	//设置默认输出低电平
	gpio_direction_output(gpios[0].gpio, 0);

	// echo pin
	{		
		//将一个GPIO转化成为中断号
		gpios[1].irq  = gpio_to_irq(gpios[1].gpio);
		/* 注册中断
		 * 参数一 ： 中断号
		 * 参数二 ： 中断处理函数
		 * 参数三 ： 中断触发类型（IRQF_TRIGGER_RISING表示上升沿触发，IRQF_TRIGGER_FALLING表示下降沿触发，最终表示双边沿触发）
		 * 参数四 ： 名字（不重要）
		 * 参数五 ： 引脚
		*/
		err = request_irq(gpios[1].irq, sr04_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, gpios[1].name, &gpios[1]);
		//设置定时器
		setup_timer(&gpios[1].key_timer, sr04_timer_func, (unsigned long)&gpios[1]);
	}

	/* 注册file_operations 	*/
	major = register_chrdev(0, "100ask_sr04", &sr04_drv);  /* /dev/gpio_desc */

	/******这里相当于命令行输入 mknod 	/dev/sr04 c 240 0 创建设备节点*****/

	//创建类，为THIS_MODULE模块创建一个类，这个类叫做100ask_sr04_class
	gpio_class = class_create(THIS_MODULE, "100ask_sr04_class");
	//如果类创建失败
	if (IS_ERR(gpio_class)) 
	{
		/*__FILE__ ：表示文件
		 *__FUNCTION__ ：当前函数名
		 *__LINE__ ：在文件的哪一行
		*/
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		//打印类创建失败
		unregister_chrdev(major, "100ask_sr04");
		//返回错误
		return PTR_ERR(gpio_class);
	}

	/*输入参数是逻辑设备的设备名，即在目录/dev目录下创建的设备名
	 *参数一 ： 在gpio_class类下面创建设备
	 *参数二 ： 无父设备的指针
	 *参数三 ： dev为主设备号+次设备号
	 *参数四 ： 没有私有数据
	*/
	device_create(gpio_class, NULL, MKDEV(major, 0), NULL, "sr04"); /* /dev/sr04 */
	
	return err;
}

/* 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 */
static void __exit sr04_exit(void)
{
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
	unregister_chrdev(major, "100ask_sr04");

	// trig pin
	gpio_free(gpios[0].gpio);

	// echo pin
	{
		//释放引脚
		free_irq(gpios[1].irq, &gpios[1]);
		//为了防止中断函数中，没有将定时器删除。而卸载驱动之后，定时器依旧在运行，所以这里需要删除定时器
		del_timer(&gpios[1].key_timer);
	}
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(sr04_init);  //确认入口函数
module_exit(sr04_exit);  //确认出口函数

/*最后我们需要在驱动中加入 LICENSE 信息和作者信息，其中 LICENSE 是必须添加的，否则的话编译的时候会报错，作者信息可以添加也可以不添加
 *这个协议要求我们代码必须免费开源，Linux遵循GPL协议，他的源代码可以开放使用，那么你写的内核驱动程序也要遵循GPL协议才能使用内核函数
 *因为指定了这个协议，你的代码也需要开放给别人免费使用，同时可以根据这个协议要求很多厂商提供源代码
 *但是很多厂商为了规避这个协议，驱动源代码很简单，复杂的东西放在应用层
*/
MODULE_LICENSE("GPL"); //指定模块为GPL协议
MODULE_AUTHOR("CSDN:qq_63922192");  //表明作者，可以不写



