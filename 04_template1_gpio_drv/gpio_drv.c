/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释。
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是应用层代码
 * 作者 ： CSDN风正豪
*/


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
	int irq;    //中断号
    char *name; //名字
    int key;    //按键值
	struct timer_list key_timer; //定时器，用于消除抖动
} ;

static struct gpio_desc gpios[2] = {
    {131, 0, "gpio_100ask_1", },
    {132, 0, "gpio_100ask_2", },
};

/* 主设备号                                                                 */
static int major = 0;
static struct class *gpio_class;   //一个类，用于创建设备节点

/* 环形缓冲区 */
#define BUF_LEN 128
static int g_keys[BUF_LEN];
static int r, w;

struct fasync_struct *button_fasync;

#define NEXT_POS(x) ((x+1) % BUF_LEN)

static int is_key_buf_empty(void)
{
	//读位置等于写位置，表示空
	return (r == w);
}

static int is_key_buf_full(void)
{
	return (r == NEXT_POS(w));
}

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

// static void key_timer_expire(struct timer_list *t)
//定时器超时函数，按键抖动消除之后进入
static void key_timer_expire(unsigned long data)
{
	/* data ==> gpio */
	// struct gpio_desc *gpio_desc = from_timer(gpio_desc, t, key_timer);
	struct gpio_desc *gpio_desc = (struct gpio_desc *)data;
	int val;
	int key;
	//读取按键
	val = gpio_get_value(gpio_desc->gpio);


	//printk("key_timer_expire key %d %d\n", gpio_desc->gpio, val);
	//读取按键值
	key = (gpio_desc->key) | (val<<8);
	//将按键值放入环形缓冲区
	put_key(key);
	//如果有人在等待这些按键的话，就需要去唤醒。在等待队列（gpio_wait）中唤醒
	wake_up_interruptible(&gpio_wait);
	//如果是异步通知，使用这个函数，发一个信号给线程
	kill_fasync(&button_fasync, SIGIO, POLL_IN);
}


/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t gpio_drv_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	//printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	int err;
	int key;
	//如果环形缓冲区没有数据，并且打开的文件flag为非阻塞
	if (is_key_buf_empty() && (file->f_flags & O_NONBLOCK))
		return -EAGAIN; //返回错误
	//如果是阻塞模式，这里将会等待is_key_buf_empty()为真。1，等待过程会改变程序状态 2，把这个程序记录在gpio_wait（队列）中
	wait_event_interruptible(gpio_wait, !is_key_buf_empty());
	//获得按键值数据，读取环形缓冲区
	key = get_key();
	/* 作用 ： 驱动层发数据给应用层
	 * buf  ： 应用层数据
	 * &key : 驱动层数据
	 * len  ：数据长度
	*/
	err = copy_to_user(buf, &key, 4);
	
	return 4;
}

static ssize_t gpio_drv_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    unsigned char ker_buf[2];
    int err;

    if (size != 2)
        return -EINVAL;

    err = copy_from_user(ker_buf, buf, size);
    
    if (ker_buf[0] >= sizeof(gpios)/sizeof(gpios[0]))
        return -EINVAL;

    gpio_set_value(gpios[ker_buf[0]].gpio, ker_buf[1]);
    return 2;    
}


static unsigned int gpio_drv_poll(struct file *fp, poll_table * wait)
{
	//printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	//将当前的进程放到gpio_wait队列中，并没有休眠
	poll_wait(fp, &gpio_wait, wait);
	//返回当前状态
	return is_key_buf_empty() ? 0 : POLLIN | POLLRDNORM;
}

static int gpio_drv_fasync(int fd, struct file *file, int on)
{
	if (fasync_helper(fd, file, on, &button_fasync) >= 0)
		return 0;
	else
		return -EIO;
}


/* 定义自己的file_operations结构体                                              */
static struct file_operations gpio_key_drv = {
	.owner	 = THIS_MODULE,
	.read    = gpio_drv_read,      //读
	.write   = gpio_drv_write,     //写
	.poll    = gpio_drv_poll,      //poll机制
	.fasync  = gpio_drv_fasync,    //异步通知机制
};

//中断函数
static irqreturn_t gpio_key_isr(int irq, void *dev_id)
{
	struct gpio_desc *gpio_desc = dev_id;
	printk("gpio_key_isr key %d irq happened\n", gpio_desc->gpio);
	//修改定时器的超时时间，每次进入中断，当前值增加HZ/5，用于消除抖动。抖动消除之后，进入 key_timer_expire 函数
	mod_timer(&gpio_desc->key_timer, jiffies + HZ/5);
	return IRQ_HANDLED;
}


/* 在入口函数 */
static int __init gpio_drv_init(void)
{
    int err;
    int i;
	//计算有多少个GPIO需要转化成中断号
    int count = sizeof(gpios)/sizeof(gpios[0]);
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	for (i = 0; i < count; i++)
	{	
		//将一个GPIO转化成为中断号
		gpios[i].irq  = gpio_to_irq(gpios[i].gpio);
		//设置定时器，给他提供定时器处理函数（key_timer_expire），还给这个处理函数提供一个参数(unsigned long)&gpios[i]
		setup_timer(&gpios[i].key_timer, key_timer_expire, (unsigned long)&gpios[i]);
	 	//timer_setup(&gpios[i].key_timer, key_timer_expire, 0);
	 	//定时器相关配置，超时时间设置为最大
		gpios[i].key_timer.expires = ~0;
		//启动定时器
		add_timer(&gpios[i].key_timer);
		/* 注册中断
		 * 参数一 ： 中断号
		 * 参数二 ： 中断处理函数
		 * 参数三 ： 中断触发类型（IRQF_TRIGGER_RISING表示上升沿触发，IRQF_TRIGGER_FALLING表示下降沿触发，最终表示双边沿触发）
		 * 参数四 ： 名字（不重要）
		 * 参数五 ： 引脚
		*/
		err = request_irq(gpios[i].irq, gpio_key_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "100ask_gpio_key", &gpios[i]);
	}

	/* 注册file_operations 	*/
	major = register_chrdev(0, "100ask_gpio_key", &gpio_key_drv);  /* /dev/gpio_desc */

	/******这里相当于命令行输入 mknod  /dev/100ask_gpio c 240 0 创建设备节点*****/

	//创建类，为THIS_MODULE模块创建一个类，这个类叫做 100ask_gpio_key_class
	gpio_class = class_create(THIS_MODULE, "100ask_gpio_key_class");
	if (IS_ERR(gpio_class))  //如果返回错误
	{
		/*__FILE__ ：表示文件
		 *__FUNCTION__ ：当前函数名
		 *__LINE__ ：在文件的哪一行
		*/
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		//注销file_operations
		unregister_chrdev(major, "100ask_gpio_key");
		//返回错误
		return PTR_ERR(gpio_class);
	}
	/* 输入参数是逻辑设备的设备名，即在目录/dev目录下创建的设备名
	 * 参数一 ： 在hello_class类下面创建设备
	 * 参数二 ： 无父设备的指针
	 * 参数三 ： dev为主设备号+次设备号
	 * 参数四 ： 没有私有数据
	*/
	device_create(gpio_class, NULL, MKDEV(major, 0), NULL, "100ask_gpio"); /* /dev/100ask_gpio */
	
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
	//注销file_operations
	unregister_chrdev(major, "100ask_gpio_key");

	for (i = 0; i < count; i++)
	{
		free_irq(gpios[i].irq, &gpios[i]);
		del_timer(&gpios[i].key_timer);
	}
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(gpio_drv_init);
module_exit(gpio_drv_exit);

MODULE_LICENSE("GPL");


