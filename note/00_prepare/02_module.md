# 驱动模块相关命令

## 加载和卸载

加载驱动模块也可以使用 `modprobe` 命令，它比 `insmod` 命令更强大，`modprobe` 命令在加载驱动模块的时候，会同时加载该模块依赖的其他模块。

```shell
insmod led_drv.ko
modprobe led_drv.ko
```

如果驱动模块是以`modprobe helloworld.ko`命令加载的，卸载的时候使用`modprobe -r helloworld.ko`命令卸载。需要注意使用 `modprobe` 卸载存在一个问题，如果**所依赖的模块被其他模块所使用，例如的 `before.ko` 还被其他的模块使用，这时候就不能使用 `modprobe` 卸载**。

```shell
rmmod led_drv
modprobe -r helloworld.ko
```

## 增加模块信息

1. 指定当前模块支持的协议，**必须写**！如果不加 LICENSE ，程序编译会出现问题。这个协议要求我们代码必须免费开源，Linux 遵循GPL 协议，他的源代码可以开放使用，那么你写的内核驱动程序也要遵循 GPL 协议才能使用内核函数。**因为指定了这个协议，你的代码也需要开放给别人免费使用，同时可以根据这个协议要求很多厂商提供源代码**。但是很多厂商为了规避这个协议，驱动源代码很简单，复杂的东西放在应用层。

```c
MODULE_LICENSE("GPL");
MODULE_LICENSE("GPL v2");
```

2. 表明当前模块作者。该参数是可选项。

```c
MODULE_AUTHOR("zyx");  //作者信息
```

3. 为内核模块提供一个简短的描述字符串，用于内核模块的元数据中**指定模块的功能和用途**，便于其他开发者、维护者或用户了解模块的目的。该参数是可选项。

```c
MODULE_DESCRIPTION("A simple kernel module that prints messages on init and exit.");
```

## 查看模块信息

如下命令可以列出**模块作者**，**模块说明**，**模块支持的参数**等信息。

```shell
modinfo led_drv.ko
```

`lsmod` 命令可以列出已经载入 Linux 内核模块。

```shell
lsmod
```

# 驱动模块传参

## 介绍

驱动模块传参是一种可以随时向内核模块传递、修改参数的方法。例如可以传递串口驱动的波特率、数据位数、校验位、停止位等参数，进行功能的设置，以此节省编译模块的时间，大大提高调试速度。

1. 传递单一的模块参数。

```c
/* 作用 : 该宏用于定义一个单一的模块参数
 * name : 参数的名称，在加载模块时通过 modprobe 或 insmod 来传递。
 * type : 参数的数据类型。
 * perm : 权限位，表示参数的访问权限。
 */
module_param(name, type, perm);

// type 常见数据类型
bool   : 布尔型
inbool : 布尔反值
charp  : 字符指针（相当于 char *,不超过 1024 字节的字符串）
short  : 短整型
ushort : 无符号短整型
int    : 整型
uint   : 无符号整型
long   : 长整型
ulong  : 无符号长整型

// perm 可选值
#define S_IRUSR 00400 /*文件所有者可读*/
#define S_IWUSR 00200 /*文件所有者可写*/
#define S_IXUSR 00100 /*文件所有者可执行*/
#define S_IRGRP 00040 /*与文件所有者同组的用户可读*/
#define S_IWGRP 00020 /*与文件所有者同组的用户可写*/
#define S_IXGRP 00010 /*与文件所有者同组的用户可执行*/
#define S_IROTH 00004 /*与文件所有者不同组的用户可读*/
#define S_IWOTH 00002 /*与文件所有者不同组的用户可写*/
#define S_IXOTH 00001 /*与文件所有者不同组的用户可可执行*/
    
/******* 示例 *******/
// 驱动程序，mymodule.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int my_param = 0;
module_param(my_param, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(my_param, "An integer parameter");
// 驱动装载
insmod mymodule.ko my_param=10
```

2. 进行多个参数传递。

```c
/* 作用 : 该宏用于定义一个数组类型的模块参数，它允许通过模块参数传递多个值。
 * name : 参数的名称，在加载模块时通过 modprobe 或 insmod 来传递。
 * type : 数组中元素的数据类型，作用与 module_param 中的 type 参数相同。
 * nump : 指向整数的指针，该整数表示数组的元素个数。当模块加载时，内核会填充这个值。
 * perm : 参数的权限，作用与 module_param 中的 perm 参数相同。
 */
module_param_array(name, type, nump, perm);

/******* 示例 *******/
// 驱动程序，mymodule.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int my_array[5] = {1, 2, 3, 4, 5};
static int my_array_size = 5;
module_param_array(my_array, int, &my_array_size, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(my_array, "An integer array");
// 驱动装载
insmod mymodule.ko my_array=1,2,3,4,5
```

3. 传递字符串类型数据。

```c
/* 作用 : 该宏用于定义一个字符串类型的模块参数。
 * name   : 参数的名称，在加载模块时通过 modprobe 或 insmod 来传递。
 * string : 存放参数值的字符数组。
 * len    : 字符数组的长度。
 * perm   : 参数的权限，作用与 module_param 中的 perm 参数相同。
 */
module_param_string(name, string, len, perm);

/******* 示例 *******/
// 驱动程序，mymodule.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static char my_string[100];
module_param_string(my_string, my_string, sizeof(my_string), S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(my_string, "A string parameter");
// 驱动装载
insmod mymodule.ko my_string="Hello, world!"
```

4. `MODULE_PARM_DESC` 宏用于为内核模块的参数提供描述性文本，它为模块的参数添加注释，便于开发人员和用户理解每个参数的用途。这个宏不会影响模块的行为，只是用于文档化和增强可读性，特别是在模块的加载或查看时，它可以通过 `modinfo` 工具查看。

```c
/* 作用 : 该宏用于定义一个单一的模块参数
 * name : 参数的名称，通常是通过 module_param、module_param_array 或 module_param_string 宏定义的参数名称。
 * description : 对该参数的描述文本，简要说明该参数的用途和如何使用它。
 */
MODULE_PARM_DESC(name, description);

/******* 示例 *******/
// 驱动程序，mymodule.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int my_param = 0;
module_param(my_param, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(my_param, "An integer parameter that controls the behavior of the module");
// 查看驱动信息
$ modinfo mymodule.ko
...
parm:           my_param: An integer parameter that controls the behavior of the module
```

## 综合示例

### 驱动代码

```c
#include <linux/init.h>
#include <linux/module.h>

static int number;     //定义int类型变量number
static char *name;     //定义char类型变量name
static int para[8];    //定义int类型的数组
static char str1[10];  //定义char类型字符串str1
static int n_para;     //定义int类型的用来记录module_param_array函数传递数组元素个数的变量n_para

module_param(number, int, S_IRUGO);                    //传递int类型的参数number，S_IRUGO表示权限为可读
MODULE_PARM_DESC(number,"This is an int parameter");
module_param(name, charp, S_IRUGO);                    //传递char类型变量name
MODULE_PARM_DESC(name,"This is an char parameter");
module_param_array(para , int , &n_para , S_IRUGO);    //传递int类型的数组变量para
MODULE_PARM_DESC(para,"This is an array type parameter");
module_param_string(str, str1 ,sizeof(str1), S_IRUGO); //传递字符串类型的变量str1
MODULE_PARM_DESC(str,"This is a string type parameter with a maximum of 9 characters");

static int __init parameter_init(void)//驱动入口函数
{
    static int i;
    printk(KERN_EMERG "%d\n",number);
    printk(KERN_EMERG "%s\n",name);                                                                                                                                                          
    for(i = 0; i < n_para; i++)
    {
        printk(KERN_EMERG "para[%d] : %d \n", i, para[i]);
    }
    printk(KERN_EMERG "%s\n",str1);
    return 0;
}
static void __exit parameter_exit(void)//驱动出口函数
{
    printk(KERN_EMERG "parameter_exit\n");
}
module_init(parameter_init);//注册入口函数
module_exit(parameter_exit);//注册出口函数
MODULE_LICENSE("GPL v2");//同意GPL开源协议
MODULE_AUTHOR("zyx"); //作者信息

```

### 测试

```c
$ insmod parameter.ko number=100 name="zyx" para=0,1,2,3 str="hello"
[11260.445618] 100
[11260.447421] zyx
[11260.449200] para[0] : 0 
[11260.451756] para[1] : 1 
[11260.461969] para[2] : 2 
[11260.465332] para[3] : 3 
[11260.467907] hello
$ modinfo parameter.ko 
filename:       /mnt/Linux_driver_example/02_parameter/parameter.ko
author:         zyx
license:        GPL v2
depends:        
vermagic:       4.9.88 SMP preempt mod_unload modversions ARMv7 p2v8 
parm:           number:This is an int parameter (int)
parm:           name:This is an char parameter (charp)
parm:           para:This is an array type parameter (array of int)
parm:           str:This is a string type parameter with a maximum of 9 characters (string)
```

# 后续需要介绍的

MODULE_ALIAS 宏