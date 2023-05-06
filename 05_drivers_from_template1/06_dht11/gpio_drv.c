/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释。
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是应用层代码
 * 作者 ： CSDN风正豪
*/

#include "asm-generic/errno-base.h"
#include "asm-generic/gpio.h"
#include "linux/jiffies.h"
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
    {115, 0, "dht11", },   //编号，引脚电平，名字
};

/* 主设备号                                                                 */
static int major = 0;
static struct class *gpio_class;    //一个类，用于创建设备节点

static u64 g_dht11_irq_time[84];
static int g_dht11_irq_cnt = 0;

/* 环形缓冲区 */
#define BUF_LEN 128
static char g_keys[BUF_LEN];
static int r, w;

struct fasync_struct *button_fasync;

static irqreturn_t dht11_isr(int irq, void *dev_id);
static void parse_dht11_datas(void);

#define NEXT_POS(x) ((x+1) % BUF_LEN)

static int is_key_buf_empty(void)
{
	return (r == w);
}

static int is_key_buf_full(void)
{
	return (r == NEXT_POS(w));
}

static void put_key(char key)
{
	if (!is_key_buf_full())
	{
		g_keys[w] = key;
		w = NEXT_POS(w);
	}
}

static char get_key(void)
{
	char key = 0;
	if (!is_key_buf_empty())
	{
		key = g_keys[r];
		r = NEXT_POS(r);
	}
	return key;
}


static DECLARE_WAIT_QUEUE_HEAD(gpio_wait);

// static void key_timer_expire(struct timer_list *t)
//定时器超时函数
static void key_timer_expire(unsigned long data)
{
	// 解析数据, 放入环形buffer, 唤醒APP
	parse_dht11_datas();
}


/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t dht11_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	int err;
	char kern_buf[2];

	//如果应用程序传入数据大小不为2字节，返回错误
	if (size != 2)
		return -EINVAL;

	g_dht11_irq_cnt = 0;

	/* 1. 发送18ms的低脉冲 */
	err = gpio_request(gpios[0].gpio, gpios[0].name);
	gpio_direction_output(gpios[0].gpio, 0);
	gpio_free(gpios[0].gpio);

	mdelay(18);  //延时18ms
	gpio_direction_input(gpios[0].gpio);  /* 引脚变为输入方向, 由上拉电阻拉为1 */

	/* 2. 注册中断 */
	//中断为双边沿触发
	err = request_irq(gpios[0].irq, dht11_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, gpios[0].name, &gpios[0]);
	mod_timer(&gpios[0].key_timer, jiffies + 10);	

	/* 3. 休眠等待数据 */
	wait_event_interruptible(gpio_wait, !is_key_buf_empty());

	//释放中断
	free_irq(gpios[0].irq, &gpios[0]);

	//printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	/* 设置DHT11 GPIO引脚的初始状态: output 1 */
	err = gpio_request(gpios[0].gpio, gpios[0].name);
	if (err)
	{
		printk("%s %s %d, gpio_request err\n", __FILE__, __FUNCTION__, __LINE__);
	}
	//引脚设置为输出，设置为初始状态
	gpio_direction_output(gpios[0].gpio, 1);
	gpio_free(gpios[0].gpio);


	/* 4. copy_to_user */
	kern_buf[0] = get_key();
	kern_buf[1] = get_key();

	printk("get val : 0x%x, 0x%x\n", kern_buf[0], kern_buf[1]);
	//如果接收到的数据有问题，表示出问题了
	if ((kern_buf[0] == (char)-1) && (kern_buf[1] == (char)-1))
	{
		//打印错误
		printk("get err val\n");
		return -EIO;
	}
	
	/* 作用 ： 驱动层发数据给应用层
	 * buf  ： 应用层数据
	 * &key : 驱动层数据
	 * len  ：数据长度
	*/
	err = copy_to_user(buf, kern_buf, 2);
	
	return 2;
}

static int dht11_release (struct inode *inode, struct file *filp)
{
	return 0;
}


/* 定义自己的file_operations结构体                                              */
static struct file_operations dht11_drv = {
	.owner	 = THIS_MODULE,
	.read    = dht11_read,
	.release = dht11_release,
};

//解析dht11数据
static void parse_dht11_datas(void)
{
	int i;
	u64 high_time;  //高脉冲时间
	unsigned char data = 0; //bit数据
	int bits = 0;  //记录已经获取多少位数据
	unsigned char datas[5]; //一串数据一共有5字节，存放dht11的数据
	int byte = 0;  //记录解析到第几个字节数据
	unsigned char crc;  //校验码

	/* 数据个数: 可能是81、82、83、84 ，因为可能受到其他中断影响导致数据丢失*/
	if (g_dht11_irq_cnt < 81)
	{
		/* 出错 */
		put_key(-1);
		put_key(-1);

		// 唤醒APP
		wake_up_interruptible(&gpio_wait);
		g_dht11_irq_cnt = 0;
		return;
	}

	// 解析数据
	for (i = g_dht11_irq_cnt - 80; i < g_dht11_irq_cnt; i+=2)
	{
		high_time = g_dht11_irq_time[i] - g_dht11_irq_time[i-1];  //高脉冲时间

		data <<= 1;

		if (high_time > 50000) //将数据进行解析
		{
			data |= 1;
		}

		bits++;

		if (bits == 8) //当bit为8，表示已经解析了一个字节数据
		{
			datas[byte] = data; //将解析好的数据放入数组
			data = 0;
			bits = 0;
			byte++;
		}
	}

	// 放入环形buffer
	crc = datas[0] + datas[1] + datas[2] + datas[3]; //计算校验码
	if (crc == datas[4]) //如果校验码正确，表示一切正常
	{
		put_key(datas[0]);
		put_key(datas[2]);
	}
	else
	{
		put_key(-1);
		put_key(-1);
	}

	g_dht11_irq_cnt = 0;
	// 唤醒APP
	wake_up_interruptible(&gpio_wait);
}

static irqreturn_t dht11_isr(int irq, void *dev_id)
{
	struct gpio_desc *gpio_desc = dev_id;
	u64 time;
	
	/* 1. 记录中断发生的时间 */
	time = ktime_get_ns();   //获取内核精确时间，单位ns
	g_dht11_irq_time[g_dht11_irq_cnt] = time;  //将精确时间放入数组

	/* 2. 累计次数 */
	g_dht11_irq_cnt++;

	/* 3. 次数足够: 解析数据, 放入环形buffer, 唤醒APP */
	if (g_dht11_irq_cnt == 84)  //如果计数值为84，表示已经获取了全部的数据
	{
		del_timer(&gpio_desc->key_timer);
		parse_dht11_datas();  //解析dht11的数据
	}

	return IRQ_HANDLED;
}


/* 在入口函数 */
static int __init dht11_init(void)
{
    int err;   //记录GPIO是否正常申请
    int i;     //用于for循环
    int count = sizeof(gpios)/sizeof(gpios[0]);  //计算需要申请的GPIO数量
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	for (i = 0; i < count; i++)
	{	
		//将引脚转换为中断号
		gpios[i].irq  = gpio_to_irq(gpios[i].gpio);

		/* 设置DHT11 GPIO引脚的初始状态: output 1 */
		//申请引脚，为其赋予名字
		err = gpio_request(gpios[i].gpio, gpios[i].name);
		//将引脚设置为输出，默认高电平
		gpio_direction_output(gpios[i].gpio, 1);
		//释放引脚
		gpio_free(gpios[i].gpio);

		setup_timer(&gpios[i].key_timer, key_timer_expire, (unsigned long)&gpios[i]);
	 	//timer_setup(&gpios[i].key_timer, key_timer_expire, 0);
		//gpios[i].key_timer.expires = ~0;
		//add_timer(&gpios[i].key_timer);
		//err = request_irq(gpios[i].irq, dht11_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "100ask_gpio_key", &gpios[i]);
	}

	/* 注册file_operations 	*/
	major = register_chrdev(0, "100ask_dht11", &dht11_drv);  /* /dev/gpio_desc */

	/******这里相当于命令行输入 mknod 	/dev/sr04 c 240 0 创建设备节点*****/

	//创建类，为THIS_MODULE模块创建一个类，这个类叫做100ask_dht11_class
	gpio_class = class_create(THIS_MODULE, "100ask_dht11_class");
	//如果类创建失败
	if (IS_ERR(gpio_class)) 
	{
		/*__FILE__ ：表示文件
		 *__FUNCTION__ ：当前函数名
		 *__LINE__ ：在文件的哪一行
		*/
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		//卸载驱动程序
		unregister_chrdev(major, "100ask_dht11");
		//返回错误
		return PTR_ERR(gpio_class);
	}

	/*输入参数是逻辑设备的设备名，即在目录/dev目录下创建的设备名
	 *参数一 ： 在gpio_class类下面创建设备
	 *参数二 ： 无父设备的指针
	 *参数三 ： dev为主设备号+次设备号
	 *参数四 ： 没有私有数据
	*/
	device_create(gpio_class, NULL, MKDEV(major, 0), NULL, "mydht11"); /* /dev/mydht11 */
	
	return err;
}

/* 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 */
static void __exit dht11_exit(void)
{
    int i;  //用于for循环
    int count = sizeof(gpios)/sizeof(gpios[0]);  //计算要释放的引脚数量
	
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
	unregister_chrdev(major, "100ask_dht11");

	for (i = 0; i < count; i++)
	{
		//free_irq(gpios[i].irq, &gpios[i]);
		//del_timer(&gpios[i].key_timer);
	}
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(dht11_init);  //确认入口函数
module_exit(dht11_exit);  //确认出口函数


/*最后我们需要在驱动中加入 LICENSE 信息和作者信息，其中 LICENSE 是必须添加的，否则的话编译的时候会报错，作者信息可以添加也可以不添加
 *这个协议要求我们代码必须免费开源，Linux遵循GPL协议，他的源代码可以开放使用，那么你写的内核驱动程序也要遵循GPL协议才能使用内核函数
 *因为指定了这个协议，你的代码也需要开放给别人免费使用，同时可以根据这个协议要求很多厂商提供源代码
 *但是很多厂商为了规避这个协议，驱动源代码很简单，复杂的东西放在应用层
*/
MODULE_LICENSE("GPL"); //指定模块为GPL协议
MODULE_AUTHOR("CSDN:qq_63922192");  //表明作者，可以不写



