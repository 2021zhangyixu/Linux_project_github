/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释。
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是应用层代码
 * 作者 ： CSDN风正豪
*/


#include "asm/cacheflush.h"
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/raw.h>
#include <linux/tty.h>
#include <linux/capability.h>
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/backing-dev.h>
#include <linux/shmem_fs.h>
#include <linux/splice.h>
#include <linux/pfn.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/uio.h>

#include <linux/uaccess.h>


static struct class *hello_class;  //hello_class类，用于创建设备节点
static int major; //主设备号，用于最后的驱动卸载
static unsigned char hello_buf[100]; //存放驱动层和应用层交互的信息

/*
 *传入参数 ：
	 *node ：
	 *filp ：
 *返回参数 : 如果成功返回0
*/
static int hello_open (struct inode *node, struct file *filp)
{
	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}
/*
 *传入参数 ：
	 *filp ：要读的文件
	 *buf ：读的数据放在哪里
	 *size ：读多大数据
	 *offset ：偏移值（一般不用）
 *返回参数 ：读到的数据长度	
*/
static ssize_t hello_read (struct file *filp, char __user *buf, size_t size, loff_t *offset)
{
	//判断size是否大于100，如果大于100，len=100，否则len=size
    unsigned long len = size > 100 ? 100 : size;
	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	/* 作用 ： 驱动层发数据给应用层
	 * buf  ： 应用层数据
	 * hello_buf : 驱动层数据
	 * len  ：数据长度
	*/
    copy_to_user(buf, hello_buf, len);

    return len;
}
/*
 *传入参数 ：
	 *filp ：要写的文件
	 *buf ：写入的数据来自于buf
	 *size ：写多大数据
	 *offset ：偏移值（一般不用）
 *返回参数 ：写的数据长度	
*/
static ssize_t hello_write(struct file *filp, const char __user *buf, size_t size, loff_t *offset)
{
	//判断size是否大于100，如果大于100，len=100，否则len=size
    unsigned long len = size > 100 ? 100 : size;
	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	/* 作用 ： 驱动层得到应用层数据
	 * buf  ： 应用层数据
	 * hello_buf : 驱动层数据
	 * len  ：数据长度
	*/
    copy_from_user(hello_buf, buf, len);

    return len;
}
/*作用 ： 应用程序关闭的时候调用这个函数
 *传入参数 ：
	 *node ：
	 *filp ：要关闭的文件
 *返回参数 ：成功返回0	
*/
static int hello_release (struct inode *node, struct file *filp)
{
	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}

//1,构造 file_operations
static const struct file_operations hello_drv = {
    .owner      = THIS_MODULE,
	.read		= hello_read,
	.write		= hello_write,
	.open		= hello_open,
    .release    = hello_release,
};


//2,注册驱动（注意，我们在入口函数中注册）

//在命令行输入insmod命令，就是注册驱动程序。之后就会进入这个入口函数
//3,入口函数
static int hello_init(void)
{
	/*将hello_drv这个驱动放在内核的第n项，中间传入的名字不重要，第三个是要告诉内核的驱动
	 *因为我们不知道第n项是否已经存放了其他驱动，就可以放在第0项，然后让系统自动往后遍历存放到空的地方
	 *major为最终存放的第n项,等下卸载程序需要使用。如果不卸载程序，可以不管这个
	*/
    major = register_chrdev(0, "100ask_hello", &hello_drv);
	//如果成功注册驱动，打印
	printk("insmod success!\n");
	
	/******这里相当于命令行输入 mknod  /dev/hello c 240 0 创建设备节点*****/
	
	//创建类，为THIS_MODULE模块创建一个类，这个类叫做hello_class
	hello_class = class_create(THIS_MODULE, "hello_class");
	if (IS_ERR(hello_class))  //如果返回错误
	{
		//打印类创建失败
		printk("failed to allocate class\n");
		//返回错误
		return PTR_ERR(hello_class);
	}
	/*在hello_class类下面创建设备
	 *无父设备的指针
	 *MKDEV传入主设备号major和此设备号0
	 *没有私有数据
	 *输入参数是逻辑设备的设备名，即在目录/dev目录下创建的设备名
	*/
    device_create(hello_class, NULL, MKDEV(major, 0), NULL, "hello");  /* /dev/hello */

   return 0;
}

//在命令行输入rmmod命令，就是注册驱动程序。之后就会进入这个出口函数
//4,出口函数
static void hello_exit(void)
{
	//销毁hello_class类下面的设备节点
    device_destroy(hello_class, MKDEV(major, 0));
	//销毁hello_class类
    class_destroy(hello_class);
	//卸载驱动程序
	//第一个参数是主设备号，第二个是名字
    unregister_chrdev(major, "100ask_hello");
	//如果成功卸载驱动，打印
	printk("rmmod success!\n");
}


module_init(hello_init); //确认入口函数
module_exit(hello_exit); //确认出口函数

/*最后我们需要在驱动中加入 LICENSE 信息和作者信息，其中 LICENSE 是必须添加的，否则的话编译的时候会报错，作者信息可以添加也可以不添加
 *这个协议要求我们代码必须免费开源，Linux遵循GPL协议，他的源代码可以开放使用，那么你写的内核驱动程序也要遵循GPL协议才能使用内核函数
 *因为指定了这个协议，你的代码也需要开放给别人免费使用，同时可以根据这个协议要求很多厂商提供源代码
 *但是很多厂商为了规避这个协议，驱动源代码很简单，复杂的东西放在应用层
*/
MODULE_LICENSE("GPL"); //指定模块为GPL协议
MODULE_AUTHOR("CSDN:qq_63922192");  //表明作者，可以不写


