/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释。
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是应用层代码
 * 作者 ： CSDN风正豪
*/

#include "acpi/acoutput.h"
#include "asm-generic/errno-base.h"
#include "asm-generic/gpio.h"
#include "asm/gpio.h"
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
	int irq;    //中断号
    char *name; //名字
    int key;    //按键值
	struct timer_list key_timer; //定时器，用于消除抖动
} ;


static struct gpio_desc gpios[] = {
    {115, 0, "ds18b20", },   //编号，引脚电平，名字
};

/* 主设备号                                                                 */
static int major = 0;
static struct class *gpio_class;  //一个类，用于创建设备节点

static spinlock_t ds18b20_spinlock;

//进行微妙级延时
static void ds18b20_udelay(int us)
{
	u64 time = ktime_get_ns();   //读出当前时间，单位ns
	while (ktime_get_ns() - time < us*1000);
}

//发出复位信号，并且等待回应
static int ds18b20_reset_and_wait_ack(void)
{
	int timeout = 100;  //释放总线，等待DS18B20回应信号的时间，为100us

	//将GPIO拉低
	gpio_set_value(gpios[0].gpio, 0);
	//延时480us
	ds18b20_udelay(480);
	//将GPIO设置为输入
	gpio_direction_input(gpios[0].gpio);

	/* 等待ACK */
	//等待DS18B20回应信号，如果读取到低电平，或者过了100us
	while (gpio_get_value(gpios[0].gpio) && timeout--)
	{
		ds18b20_udelay(1);
	}

	//如果等待时间过长，一直没有低电平，表示设备出问题
	if (timeout == 0)
		return -EIO;

	/* 等待ACK结束 */
	timeout = 300;  //等待ACK结束时间，等待300us
	//如果读取到了ACK，或者等待超时
	while (!gpio_get_value(gpios[0].gpio) && timeout--)
	{
		ds18b20_udelay(1);
	}

	//如果等待超时，返回错误
	if (timeout == 0)
		return -EIO;
	
	return 0;

}

//给DS18B20发送数据
static void ds18b20_send_cmd(unsigned char cmd)
{
	int i;

	//设置为输出引脚，输出高电平
	gpio_direction_output(gpios[0].gpio, 1);

	for (i = 0; i < 8; i++)
	{
		if (cmd & (1<<i)) 
		{
			/* 发送1 */
			gpio_direction_output(gpios[0].gpio, 0);  //表示开始发送信号
			ds18b20_udelay(2);
			gpio_direction_output(gpios[0].gpio, 1);  //输出1，维持超过59us
			ds18b20_udelay(60);
		}
		else
		{
			/* 发送0 */
			gpio_direction_output(gpios[0].gpio, 0);  //包含开始发送的信号，和信号为0.整个过程超过60us
			ds18b20_udelay(60);		
			gpio_direction_output(gpios[0].gpio, 1);  //最终要拉高成默认高电平
		}
	}
}

//DS18B20读取数据
static void ds18b20_read_data(unsigned char *buf)
{
	int i;
	unsigned char data = 0;

	//将GPIO设置为输出高电平
	gpio_direction_output(gpios[0].gpio, 1);
	for (i = 0; i < 8; i++)
	{
		//GPIO输出2us低电平，表示开始交互数据
		gpio_direction_output(gpios[0].gpio, 0);
		ds18b20_udelay(2);
		//将GPIO设置为输入，开始读取数据
		gpio_direction_input(gpios[0].gpio);
		ds18b20_udelay(15);   //因为读取数据需要经过15us之后，所以这里延时时间需要大于13us即可
		if (gpio_get_value(gpios[0].gpio))
		{
			data |= (1<<i);
		}
		//等待时间要超过45us，这里选择50us
		ds18b20_udelay(50);
		//将GPIO重新设置为输出高电平
		gpio_direction_output(gpios[0].gpio, 1);
	}

	buf[0] = data;
}

/********************************************************/  
/*DS18B20的CRC8校验程序*/  
/********************************************************/   
/* 参考: https://www.cnblogs.com/yuanguanghui/p/12737740.html */   
static unsigned char calcrc_1byte(unsigned char abyte)   
{   
	unsigned char i,crc_1byte;     
	crc_1byte=0;                //设定crc_1byte初值为0  
	for(i = 0; i < 8; i++)   
	{   
		if(((crc_1byte^abyte)&0x01))   
		{   
			crc_1byte^=0x18;     
			crc_1byte>>=1;   
			crc_1byte|=0x80;   
		}         
		else     
			crc_1byte>>=1;   

		abyte>>=1;         
	}   
	return crc_1byte;   
}

/* 参考: https://www.cnblogs.com/yuanguanghui/p/12737740.html */   
static unsigned char calcrc_bytes(unsigned char *p,unsigned char len)  
{  
	unsigned char crc=0;  
	while(len--) //len为总共要校验的字节数  
	{  
		crc=calcrc_1byte(crc^*p++);  
	}  
	return crc;  //若最终返回的crc为0，则数据传输正确  
}  


static int ds18b20_verify_crc(unsigned char *buf)
{
    unsigned char crc;

	crc = calcrc_bytes(buf, 8);

    if (crc == buf[8])
		return 0;
	else
		return -1;
}

static void ds18b20_calc_val(unsigned char ds18b20_buf[], int result[])
{
	unsigned char tempL=0,tempH=0;
	unsigned int integer;
	unsigned char decimal1,decimal2,decimal;

	tempL = ds18b20_buf[0]; //读温度低8位
	tempH = ds18b20_buf[1]; //读温度高8位

	if (tempH > 0x7f)      							//最高位为1时温度是负
	{
		tempL    = ~tempL;         				    //补码转换，取反加一
		tempH    = ~tempH+1;      
		integer  = tempL/16+tempH*16;      			//整数部分
		decimal1 = (tempL&0x0f)*10/16; 			//小数第一位
		decimal2 = (tempL&0x0f)*100/16%10;			//小数第二位
		decimal  = decimal1*10+decimal2; 			//小数两位
	}
	else
	{
		integer  = tempL/16+tempH*16;      				//整数部分
		decimal1 = (tempL&0x0f)*10/16; 					//小数第一位
		decimal2 = (tempL&0x0f)*100/16%10;				//小数第二位
		decimal  = decimal1*10+decimal2; 				//小数两位
	}
	result[0] = integer;
	result[1] = decimal;
}

/* 实现对应的open/read/write等函数，填入file_operations结构体                   */
static ssize_t ds18b20_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	unsigned long flags;
	int err;
	unsigned char kern_buf[9];
	int i;
	int result_buf[2];

	if (size != 8)
		return -EINVAL;

	/* 1. 启动温度转换 */
	/* 1.1 关中断 */
	spin_lock_irqsave(&ds18b20_spinlock, flags);

	/* 1.2 发出reset信号并等待回应 */
	err = ds18b20_reset_and_wait_ack();
	//如果DS18B20没有回应
	if (err)
	{
		spin_unlock_irqrestore(&ds18b20_spinlock, flags);
		printk("ds18b20_reset_and_wait_ack err\n");
		return err;
	}

	/* 1.3 发出命令: skip rom, 0xcc */
	ds18b20_send_cmd(0xcc);

	/* 1.4 发出命令: 启动温度转换, 0x44 */
	ds18b20_send_cmd(0x44);

	/* 1.5 恢复中断 */
	spin_unlock_irqrestore(&ds18b20_spinlock, flags);

	/* 2. 等待温度转换成功 : 可能长达1s */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(msecs_to_jiffies(1000));

	/* 3. 读取温度 */
	/* 3.1 关中断 */
	spin_lock_irqsave(&ds18b20_spinlock, flags);

	/* 3.2 发出reset信号并等待回应 */
	err = ds18b20_reset_and_wait_ack();
	if (err)
	{
		spin_unlock_irqrestore(&ds18b20_spinlock, flags);
		printk("ds18b20_reset_and_wait_ack second err\n");
		return err;
	}
	/* 3.3 发出命令: skip rom, 0xcc */
	ds18b20_send_cmd(0xcc);

	/* 3.4 发出命令: read scratchpad, 0xbe */
	ds18b20_send_cmd(0xbe);

	/* 3.5 读9字节数据 */
	for (i = 0; i < 9; i++)
	{
		ds18b20_read_data(&kern_buf[i]);
	}

	/* 3.6 恢复中断 */
	spin_unlock_irqrestore(&ds18b20_spinlock, flags);

	/* 3.7 计算CRC验证数据 */
	err = ds18b20_verify_crc(kern_buf);
	if (err)
	{
		printk("ds18b20_verify_crc err\n");
		return err;
	}

	/* 4. copy_to_user */
	ds18b20_calc_val(kern_buf, result_buf);
	
	err = copy_to_user(buf, result_buf, 8);
	return 8;
}


/* 定义自己的file_operations结构体                                              */
static struct file_operations gpio_key_drv = {
	.owner	 = THIS_MODULE,
	.read    = ds18b20_read,
};


/* 在入口函数 */
static int __init ds18b20_init(void)
{
    int err;
    int i;
    int count = sizeof(gpios)/sizeof(gpios[0]);  //计算需要申请的GPIO数量
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	spin_lock_init(&ds18b20_spinlock);
	
	for (i = 0; i < count; i++)
	{		
		//申请引脚，为其赋予名字
		err = gpio_request(gpios[i].gpio, gpios[i].name);
		//将GPIO设置为输出，默认高电平
		gpio_direction_output(gpios[i].gpio, 1);
	}

	/* 注册file_operations 	*/
	major = register_chrdev(0, "100ask_ds18b20", &gpio_key_drv);  /* /dev/gpio_desc */
	
	/******这里相当于命令行输入 mknod 	/dev/sr04 c 240 0 创建设备节点*****/

	//创建类，为THIS_MODULE模块创建一个类，这个类叫做100ask_ds18b20_class
	gpio_class = class_create(THIS_MODULE, "100ask_ds18b20_class");
	if (IS_ERR(gpio_class)) 
	{
		/*__FILE__ ：表示文件
		 *__FUNCTION__ ：当前函数名
		 *__LINE__ ：在文件的哪一行
		*/
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		//卸载驱动程序
		unregister_chrdev(major, "100ask_ds18b20");
		//返回错误
		return PTR_ERR(gpio_class);
	}

	/*输入参数是逻辑设备的设备名，即在目录/dev目录下创建的设备名
	 *参数一 ： 在gpio_class类下面创建设备
	 *参数二 ： 无父设备的指针
	 *参数三 ： dev为主设备号+次设备号
	 *参数四 ： 没有私有数据
	*/
	device_create(gpio_class, NULL, MKDEV(major, 0), NULL, "myds18b20"); /* /dev/myds18b20 */
	
	return err;
}

/* 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 */
static void __exit ds18b20_exit(void)
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
	unregister_chrdev(major, "100ask_ds18b20");

	for (i = 0; i < count; i++)
	{
		//释放GPIO
		gpio_free(gpios[i].gpio);
	}
}


/* 7. 其他完善：提供设备信息，自动创建设备节点                                     */

module_init(ds18b20_init);  //确认入口函数
module_exit(ds18b20_exit);  //确认入口函数



/*最后我们需要在驱动中加入 LICENSE 信息和作者信息，其中 LICENSE 是必须添加的，否则的话编译的时候会报错，作者信息可以添加也可以不添加
 *这个协议要求我们代码必须免费开源，Linux遵循GPL协议，他的源代码可以开放使用，那么你写的内核驱动程序也要遵循GPL协议才能使用内核函数
 *因为指定了这个协议，你的代码也需要开放给别人免费使用，同时可以根据这个协议要求很多厂商提供源代码
 *但是很多厂商为了规避这个协议，驱动源代码很简单，复杂的东西放在应用层
*/
MODULE_LICENSE("GPL"); //指定模块为GPL协议
MODULE_AUTHOR("CSDN:qq_63922192");  //表明作者，可以不写



