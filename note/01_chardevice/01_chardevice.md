# 绑定字符设备

## 设备号

### 申请设备号

在 Linux 驱动中可以使用以下两种方法进行设备号的申请：

1. 通过 `register_chrdev_region(dev_t from, unsigned count, const char *name)` 函数进行**静态申请**设备号。

```c
/* 作用 : 静态申请设备号，对指定好的设备号进行申请
 * from  : 起始设备号，这个参数指定从哪个设备号开始分配
 * count : 分配的设备号数量
 * name  : 设备名称，这个名称是字符串形式，用于标识设备，可使用 cat /proc/devices 命令查看
 * 返回值 : 申请设备号成功返回0，失败返回负数错误码
 */
int register_chrdev_region(dev_t from, unsigned count, const char *name);
```

2. 通过 `alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count,const char *name)` 函数进行**动态申请**设备号。**推荐使用 `alloc_chrdev_region`，因为它简化了设备号管理过程并减少了出错的机会。**

```c
/* 作用 : 动态申请设备号
 * dev       : 返回分配的设备号。该参数是一个指向 dev_t 类型的指针，分配后的设备号会保存在此处
 * baseminor : 最小的 minor 号（通常从 0 开始），minor 号范围由 count 控制
 * count     : 要申请的设备号数量
 * name      : 设备名称，这个名称是字符串形式，用于标识设备，可使用 cat /proc/devices 命令查看
 * 返回值 : 申请设备号成功返回0，失败返回负数错误码
 */
int alloc_chrdev_region(dev_t *dev, unsigned int baseminor, unsigned int count, const char *name);
```

3. 设备号 `dev_t` 本质上就是一个**无符号的 32 位整形类型，其中高 12 位表示主设备号，低 20 位表示次设备号**。如在**静态申请**设备号时需要将指定的**主设备号和从设备号通过 `MKDEV(ma,mi)` 宏进行设备号的转换**，在**动态申请**设备号时可以**用 `MAJOR(dev)` 和 `MINOR(dev)` 宏将动态申请的设备号转化为主设备号和从设备号**。

```c
#define MINORBITS 20 /*次设备号位数*/
#define MINORMASK ((1U << MINORBITS) - 1) /*次设备号掩码*/
#define MAJOR(dev) ((unsigned int) ((dev) >> MINORBITS))/*dev 右移 20 位得到主设备号*/
#define MINOR(dev) ((unsigned int) ((dev) & MINORMASK)) /*与次设备掩码与，得到次设备号*/
#define MKDEV(ma,mi) (((ma) << MINORBITS) | (mi))/*MKDEV 宏将主设备号（ma）左移 20 位，然后与次设备号（mi）相与，得到设备号*/
```

### 注销设备号

无论是静态申请设备号还是动态申请设备号，都是采用如下 API 进行注销。

```c
/* 作用 : 注销申请到的设备号
 * from  : 设备号的起始值，通常是通过 register_chrdev_region 或 alloc_chrdev_region 返回的设备号
 * count : 需要注销的设备号数量。这个值应该与最初分配时的设备号数量相同
 */
void unregister_chrdev_region(dev_t from, unsigned int count);
```

### 综合示例

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
static int major;                //定义静态加载方式时的主设备号参数major
static int minor;                //定义静态加载方式时的次设备号参数minor
module_param(major,int,S_IRUGO); //通过驱动模块传参的方式传递主设备号参数major
module_param(minor,int,S_IRUGO); //通过驱动模块传参的方式传递次设备号参数minor
static dev_t dev_num;            //定义dev_t类型(32位大小)的变量dev_num

static int __init dev_t_init(void)//驱动入口函数
{
	int ret;//定义int类型的变量ret，用来判断函数返回值
	/*以主设备号进行条件判断，即如果通过驱动传入了major参数则条件成立，进入以下分支*/
	if(major){
    	dev_num = MKDEV(major,minor);//通过MKDEV函数将驱动传参的主设备号和次设备号转换成dev_t类型的设备号
    	printk("major is %d\n",major);
    	printk("minor is %d\n",minor);
    	ret = register_chrdev_region(dev_num,1,"chrdev_name");//通过静态方式进行设备号注册
        if(ret < 0){
            printk("register_chrdev_region is error\n");
        }
        printk("register_chrdev_region is ok\n");
    }
	/*如果没有通过驱动传入major参数，则条件成立，进入以下分支*/
    else{
        ret = alloc_chrdev_region(&dev_num,0,1,"chrdev_num");//通过动态方式进行设备号注册
        if(ret < 0){
            printk("alloc_chrdev_region is error\n");
        }                                                                                                                                              
        printk("alloc_chrdev_region is ok\n");
        major=MAJOR(dev_num);//通过MAJOR()函数进行主设备号获取
        minor=MINOR(dev_num);//通过MINOR()函数进行次设备号获取
        printk("major is %d\n",major);
        printk("minor is %d\n",minor);
    }
    return 0;
}

static void __exit dev_t_exit(void)//驱动出口函数
{
    unregister_chrdev_region(dev_num,1);//注销字符驱动
    printk("module exit \n");
}

module_init(dev_t_init);  //注册入口函数
module_exit(dev_t_exit);  //注册出口函数
MODULE_LICENSE("GPL v2"); //同意GPL开源协议
MODULE_AUTHOR("topeet");  //作者信息
```

## 字符设备

### 字符设备初始化

对字符设备进行初始化。

```c
/* 作用 : 初始化字符设备
 * cdev : 指向 struct cdev 结构体的指针，表示要初始化的字符设备。struct cdev 结构体用于存储与设备相关的信息，尤其是文件操作的接口
 * fops : 指向 struct file_operations 结构体的指针
 */
void cdev_init(struct cdev *cdev, const struct file_operations *fops);

/****** 示例 ******/
static struct cdev cdev_test;
static struct file_operations cdev_test_ops = {
    // 将 owner 字段指向本模块，有助于内核跟踪和管理模块的加载和卸载，确保内核能够正确处理该模块的资源，避免潜在的资源竞争或使用未初始化的资源
	.owner=THIS_MODULE, 
};
cdev_init(&cdev_test,&cdev_test_ops);
```

### 注册字符设备

将初始化好的字符设备**添加到内核**中。

```c
/* 作用 : 将初始化好的字符设备（cdev）结构体添加到内核
 * cdev  : 在使用 cdev_init() 函数初始化的字符设备结构体，该结构体包含了与字符设备相关的操作函数指针，指定了设备的 I/O 操作方式（如 open, close, read, write 等）
 * dev   : 设备号，类型为 dev_t
 * count : 表示该设备号区域中设备的数量。通常是 1，表示只有一个设备
 * 返回值 : 将字符设备添加到内核成功返回0，失败返回负数错误码
 */
int cdev_add(struct cdev *cdev, dev_t dev, unsigned count);
```

### 注销设备号

将 `cdev_add()` 函数注册到内核中的字符设备（cdev）结构体删除。

```c
/* 作用 : 删除已经注册到内核中的字符设备（cdev）结构体
 * cdev : 指向 struct cdev 的指针，表示要删除的字符设备结构体
 */
void cdev_del(struct cdev *cdev);
```

## 设备节点

系统通过设备号对设备进行查找，而**字符设备注册到内核之后，并不能直接进行设备文件操作命令（open、close、read、write等），需要相应的设备文件作为桥梁以此来进行设备的访问**。

### 手动创建设备节点

我们可以调用 `mknod` 命令手动创建设备节点，该命令的格式如下

```shell
# NAME: 要创建的节点名称
# TYPE: b 表示块设备，c 表示字符设备，p 表示管道
# MAJOR：要链接设备的主设备号
# MINOR: 要链接设备的从设备号
mknod NAME TYPE MAJOR MINOR

# 示例
mknod /dev/device_test c 234 0
```

### 自动创建设备节点

设备文件的自动创建是利用 `udev(mdev)` 机制来实现。在驱动中首先使用 `class_create(…)` 函数对 class 进行创建，这个类存放于

`/sys/class/` 目录下，之后使用 `device_create(…)` 函数创建相应的设备。

#### 设备类

1. 在 Linux 内核中用于创建设备类别（class）

```c
/* 作用 : 在 Linux 内核中用于创建设备类别（class）
 * owner : 该参数指定拥有这个类别的内核模块，通常使用 THIS_MODULE 宏来表示当前模块。owner 字段用于管理模块的生命周期，确保在模块卸载时，类别及其相关资源会被正确清理
 * name  : 该参数是类别的名称，用于在 sysfs 中创建一个相应的目录（通常是 /sys/class/<name>），用户可以通过该目录来查看和操作设备的属性
 * 返回值 : 返回一个指向 struct class 的指针，表示创建的设备类别。如果函数调用失败，将返回 NULL
 */
struct class *class_create(struct module *owner, const char *name);
```

2. 销毁先前通过 `class_create()` 创建的设备类别。

```c
/* 作用 : 销毁先前创建的设备类别（class）
 * owner : 要销毁的设备类别，通常是在调用 class_create 创建的类别
 */
void class_destroy(struct class *class);
```

#### 设备实例

1. 在 `/sys/class/your_class/fmt` 类中下创建一个设备实例，该实例同时也会出现在 `/dev/fmt` 中。

```c
/* 作用 : 为已创建的设备类别（class）创建设备实例
 * class   : 这是设备所属的类别，设备会被添加到该类别下，类别通过 class_create 创建
 * parent  : 设备的父设备，通常设置为 NULL，表示没有父设备
 * devt    : 指定创建设备的设备号
 * drvdata : 指向设备私有数据的指针，可以在设备驱动中存储与设备相关的任意数据，通常用来存储设备的内部状态或指向设备特定资源的指针，没有则指定为 NULL
 * fmt     : 添加到系统的设备节点名称
 * 返回值 : 返回一个指向 struct device 的指针，表示创建的设备实例，如果函数调用失败，将返回 ERR_PTR(-ENOMEM) 或其他错误指针
 */
struct device *device_create(struct class *class, struct device *parent, dev_t devt,
                             void *drvdata, const char *fmt, ...);
```

2. 销毁之前通过 `device_create()` 创建的设备实例。

```c
/* 作用 : 销毁之前通过 device_create() 创建的设备实例
 * class : 要销毁的设备所属的类别
 * devt  : 要销毁的设备的设备号
 */
void device_destroy(struct class *class, dev_t devt);
```

