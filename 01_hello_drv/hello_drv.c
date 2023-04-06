//#include <stdio.h>
#include <linux/mm.h>
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
#include <linux/module.h>
#include <linux/uaccess.h>

static int major;

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
    printk("%s %s %d\n",__FILE__,__FUNCTION__, __LINE__);
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
static	ssize_t hello_read(struct file *filp, char __user *buf, size_t size, loff_t *offset)
{
	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
    printk("%s %s %d\n",__FILE__,__FUNCTION__, __LINE__);
    return size;
}

/*
 *传入参数 ：
	 *filp ：要写的文件
	 *buf ：写入的数据来自于buf
	 *size ：写多大数据
	 *offset ：偏移值（一般不用）
 *返回参数 ：写的数据长度	
*/
static	ssize_t hello_write(struct file *filp, const char __user *buf, size_t size, loff_t *offset)
{
	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
    printk("%s %s %d\n",__FILE__,__FUNCTION__, __LINE__);
    return size;
}

/*作用 ： 应用程序关闭的时候调用这个函数
 *传入参数 ：
	 *node ：
	 *filp ：要关闭的文件
 *返回参数 ：成功返回0	
*/
static	int hello_release (struct inode *node, struct file *filp)
{
	/*__FILE__ ：表示文件
	 *__FUNCTION__ ：当前函数名
	 *__LINE__ ：在文件的哪一行
	*/
    printk("%s %s %d\n",__FILE__,__FUNCTION__, __LINE__);
    return 0;
}

//1,构造 file_operations 
static const struct file_operations  hello_drv = {
    .owner      = THIS_MODULE,
	.read		= hello_read,
	.write		= hello_write,
	.open		= hello_open,
    .release    = hello_release
};

//2,注册驱动（注意，我们在入口函数中注册）

//3,入口函数
static int  hello_init(void)
{
	/*将hello_drv这个驱动放在内核的第n项，中间传入的名字不重要，第三个是要告诉内核的驱动
	 *因为我们不知道第n项是否已经存放了其他驱动，就可以放在第0项，然后让系统自动往后遍历存放到空的地方
	 *major为最终存放的第n项,等下卸载程序需要使用。如果不卸载程序，可以不管这个
	*/
    major = register_chrdev(0,"100ask_hello",&hello_drv);
    return 0;
}

//4,出口函数
static void  hello_exit(void)
{
	//卸载驱动程序
	//第一个参数是主设备号，第二个是名字
    unregister_chrdev(major,"100ask_hello");
}


module_init(hello_init); //确认入口函数
module_exit(hello_exit); //确认出口函数
MODULE_LICENSE("GPL"); //指定模块为GPL协议


