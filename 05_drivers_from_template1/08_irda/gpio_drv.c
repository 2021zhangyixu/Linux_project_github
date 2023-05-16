/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释。
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是应用层代码
 * 作者 ： CSDN风正豪
*/

#include "asm-generic/errno-base.h"
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

//描述一个引脚
struct gpio_desc{
	int gpio;   //引脚编号
	int irq;    //中断号
    char *name; //名字
    int key;    //按键值
	struct timer_list key_timer; //定时器，用于消除抖动
} ;


static struct gpio_desc gpios[] = {
    {115, 0, "irda", },   //编号，引脚电平，名字
};

/* 主设备号                                                                 */
static int major = 0;
static struct class *gpio_class;

/* 环形缓冲区 */
#define BUF_LEN 128
static unsigned char g_keys[BUF_LEN];
static int r, w;

struct fasync_struct *button_fasync;

static u64 g_irda_irq_times[68];
static int g_irda_irq_cnt = 0;

#define NEXT_POS(x) ((x+1) % BUF_LEN)  //将数据保持环形

//判断环形缓冲区是否为空
static int is_key_buf_empty(void)
{
	//如果读位置和写位置相同，那么表示环形缓冲区为空
	return (r == w);
}

static int is_key_buf_full(void)
{
	return (r == NEXT_POS(w));
}

static void put_key(unsigned char key)
{
	if (!is_key_buf_full())
	{
		g_keys[w] = key;
		w = NEXT_POS(w);
	}
}

static unsigned char get_key(void)
{
	unsigned char key = 0;
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
	/* 超时 */
	g_irda_irq_cnt = 0;
	put_key(-1);
	put_key(-1);
	wake_up_interruptible(&gpio_wait);
	kill_fasync(&button_fasync, SIGIO, POLL_IN);
}


/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t irda_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	unsigned char kern_buf[2] ;
	int err;

	//如果传入的数据不是2字节，返回错误
	if (size != 2)
		return -EINVAL;

	//
	if (is_key_buf_empty() && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;
	
	wait_event_interruptible(gpio_wait, !is_key_buf_empty());
	kern_buf[0] = get_key();  /* 哪一个遥控器 */
	kern_buf[1] = get_key();  /* 遥控器传来的数据     */

	if (kern_buf[0] == (unsigned char)-1  && kern_buf[1] == (unsigned char)-1)
		return -EIO;
	
	/* 作用 ： 驱动层发数据给应用层
	 * buf  ： 应用层数据
	 * &key : 驱动层数据
	 * len  ：数据长度
	*/
	err = copy_to_user(buf, kern_buf, 2);
	
	return 2;
}


static unsigned int irda_poll(struct file *fp, poll_table * wait)
{
	//printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	poll_wait(fp, &gpio_wait, wait);
	return is_key_buf_empty() ? 0 : POLLIN | POLLRDNORM;
}

static int irda_fasync(int fd, struct file *file, int on)
{
	if (fasync_helper(fd, file, on, &button_fasync) >= 0)
		return 0;
	else
		return -EIO;
}


/* 定义自己的file_operations结构体                                              */
static struct file_operations gpio_key_drv = {
	.owner	 = THIS_MODULE,
	.read    = irda_read,
	.poll    = irda_poll,
	.fasync  = irda_fasync,
};

//进行数据解析
static void parse_irda_datas(void)
{
	u64 time;
	int i;
	int m, n;
	unsigned char datas[4];
	unsigned char data = 0;
	int bits = 0;
	int byte = 0;

	/* 1. 判断前导码 : 9ms的低脉冲, 4.5ms高脉冲  */
	time = g_irda_irq_times[1] - g_irda_irq_times[0];
	//如果前置低脉冲时间小于8ms，或者大于10ms，表示出错误了
	if (time < 8000000 || time > 10000000)
	{
		goto err;
	}

	time = g_irda_irq_times[2] - g_irda_irq_times[1];
	//如果前置高脉冲时间不在3.5ms到5.5ms之内，表示出问题了
	if (time < 3500000 || time > 55000000)
	{
		goto err;
	}

	/* 2. 解析数据 */
	for (i = 0; i < 32; i++)
	{
		m = 3 + i*2;
		n = m+1;
		time = g_irda_irq_times[n] - g_irda_irq_times[m];
		data <<= 1;
		bits++;
		//两个数据之间超过1ms
		if (time > 1000000)
		{
			/* 得到了数据1 */
			data |= 1;
		}

		if (bits == 8)
		{
			datas[byte] = data;
			byte++;
			data = 0;
			bits = 0;
		}
	}

	/* 判断数据正误 */
	datas[1] = ~datas[1];
	datas[3] = ~datas[3];
	
	if ((datas[0] != datas[1]) || (datas[2] != datas[3]))
	{
		//打印数据校验存在问题
		printk("data verify err: %02x %02x %02x %02x\n", datas[0], datas[1], datas[2], datas[3]);
		goto err;
	}

	put_key(datas[0]);  //设备
	put_key(datas[2]);  //数据
	wake_up_interruptible(&gpio_wait);
	kill_fasync(&button_fasync, SIGIO, POLL_IN);
	return;

err:
	g_irda_irq_cnt = 0;
	put_key(-1);
	put_key(-1);
	wake_up_interruptible(&gpio_wait);
	kill_fasync(&button_fasync, SIGIO, POLL_IN);
}

static irqreturn_t gpio_key_isr(int irq, void *dev_id)
{
	struct gpio_desc *gpio_desc = dev_id;
	u64 time;
	
	/* 1. 记录中断发生的时刻 */	
	time = ktime_get_ns();  //获取精确时间，单位ns
	g_irda_irq_times[g_irda_irq_cnt] = time;  

	/* 2. 累计中断次数 */
	g_irda_irq_cnt++;

	/* 3. 次数达标后, 删除定时器, 解析数据, 放入buffer, 唤醒APP */
	if (g_irda_irq_cnt == 68)
	{
		parse_irda_datas();
		del_timer(&gpio_desc->key_timer);
		g_irda_irq_cnt = 0;
	}

	/* 4. 启动定时器，100ms进入中断 */
	mod_timer(&gpio_desc->key_timer, jiffies + msecs_to_jiffies(100));
	return IRQ_HANDLED;
}


/* 在入口函数 */
static int __init irda_init(void)
{
    int err;
    int i;
    int count = sizeof(gpios)/sizeof(gpios[0]);  //计算需要使用的GPIO数量

	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	for (i = 0; i < count; i++)
	{		
		//申请GPIO中断
		gpios[i].irq  = gpio_to_irq(gpios[i].gpio);

		//如果一直没有等到中断，定时器可以用于进行容错操作
		setup_timer(&gpios[i].key_timer, key_timer_expire, (unsigned long)&gpios[i]);
	 	//timer_setup(&gpios[i].key_timer, key_timer_expire, 0);   //高版本的内核是使用这个函数注册定时器
	 	//申请双边沿中断
		err = request_irq(gpios[i].irq, gpio_key_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, gpios[i].name, &gpios[i]);
	}

	/* 注册file_operations 	*/
	major = register_chrdev(0, "100ask_irda", &gpio_key_drv);  /* /dev/gpio_desc */
	
	/******这里相当于命令行输入 mknod 	/dev/sr04 c 240 0 创建设备节点*****/

	//创建类，为THIS_MODULE模块创建一个类，这个类叫做100ask_irda_class
	gpio_class = class_create(THIS_MODULE, "100ask_irda_class");
	//如果类创建失败
	if (IS_ERR(gpio_class)) 
	{
		/*__FILE__ ：表示文件
		 *__FUNCTION__ ：当前函数名
		 *__LINE__ ：在文件的哪一行
		*/
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		//卸载驱动程序
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
	device_create(gpio_class, NULL, MKDEV(major, 0), NULL, "irda"); /* /dev/irda */
	
	return err;
}

/* 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 */
static void __exit irda_exit(void)
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
	unregister_chrdev(major, "100ask_irda");

	for (i = 0; i < count; i++)
	{
		//释放GPIO
		free_irq(gpios[i].irq, &gpios[i]);
		//删除定时器
		del_timer(&gpios[i].key_timer);
	}
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(irda_init);  //确认入口函数
module_exit(irda_exit);  //确认出口函数


/*最后我们需要在驱动中加入 LICENSE 信息和作者信息，其中 LICENSE 是必须添加的，否则的话编译的时候会报错，作者信息可以添加也可以不添加
 *这个协议要求我们代码必须免费开源，Linux遵循GPL协议，他的源代码可以开放使用，那么你写的内核驱动程序也要遵循GPL协议才能使用内核函数
 *因为指定了这个协议，你的代码也需要开放给别人免费使用，同时可以根据这个协议要求很多厂商提供源代码
 *但是很多厂商为了规避这个协议，驱动源代码很简单，复杂的东西放在应用层
*/
MODULE_LICENSE("GPL"); //指定模块为GPL协议
MODULE_AUTHOR("CSDN:qq_63922192");  //表明作者，可以不写



