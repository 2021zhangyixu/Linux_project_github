# 私有数据

## 私有数据定义

文件私有数据就是将 struct file 结构体中的 private_data 指向设备结构体，**一般是在 open 的时候赋值，read、write 时使用**。

```c
typedef struct chr_drv
{
	int major;                 /*< Master device number */
	int minor;                 /*< Secondary device number */
	dev_t dev_num;             /*< Device number of type dev_t consisting of primary device number and secondary device number */
	struct cdev chr_cdev;      /*< Character device structure */
	struct class *chr_class;   /*< Device class */
    struct device *chr_device; /*< Device instance */
    char kbuf[32];             /*< Kernel buffer*/
}chr_drv;

static int chrdev_open(struct inode *inode, struct file *file)
{
    file->private_data = &s_char_drv;
    // ...
	return 0;
}
static ssize_t chrdev_read(struct file *file,char __user *buf, size_t size, loff_t *off)
{
    chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    // ...
}

static ssize_t chrdev_write(struct file *file,const char __user *buf,size_t size,loff_t *off)
{
    chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    // ...
}
```

## 一个驱动兼容多个次设备

在 Linux 中，使用**主设备号来表示对应某一类驱动**，使用**次设备号来表示这类驱动下的各个设备**。假如现在驱动要支持的主设备号相同，但是次设备号不同的设备，那么我们就可以利用私有数据来实现。

目前硬件暂时不支持，后续加上硬件后看北京讯为电子的**第 16 章节 一个驱动兼容不同设备实验**。

```c
/* 作用 : 通过结构体变量中某个成员的首地址获取到整个结构体变量的首地址
 * to     : 指向结构体成员的指针
 * type   : 结构体的类型
 * member : 结构体中的成员的名字
 */
container_of(ptr, type, member);
```

# 