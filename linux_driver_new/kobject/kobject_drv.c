#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/configfs.h>
#include <linux/kernel.h>
#include <linux/kobject.h>

// 定义kobject结构体指针，用于表示第一个自定义内核对象
struct kobject *mykobject01;
// 定义kobject结构体指针，用于表示第二个自定义内核对象
struct kobject *mykobject02;
// 定义kset结构体指针，用于表示自定义内核对象的集合
struct kset *mykset;
// 定义kobj_type结构体，用于定义自定义内核对象的类型
struct kobj_type mytype;

// 模块的初始化函数
static int mykobj_init(void)
{
    int ret;

    // 创建并添加kset，名称为"mykset"，父kobject为NULL，属性为NULL
    mykset = kset_create_and_add("mykset", NULL, NULL);

    // 为mykobject01分配内存空间，大小为struct kobject的大小，标志为GFP_KERNEL
    mykobject01 = kzalloc(sizeof(struct kobject), GFP_KERNEL);
    // 将mykset设置为mykobject01的kset属性
    mykobject01->kset = mykset;
    // 初始化并添加mykobject01，类型为mytype，父kobject为NULL，格式化字符串为"mykobject01"
    ret = kobject_init_and_add(mykobject01, &mytype, NULL, "%s", "mykobject001");

    // 为mykobject02分配内存空间，大小为struct kobject的大小，标志为GFP_KERNEL
    mykobject02 = kzalloc(sizeof(struct kobject), GFP_KERNEL);
    // 将mykset设置为mykobject02的kset属性
    mykobject02->kset = mykset;
    // 初始化并添加mykobject02，类型为mytype，父kobject为NULL，格式化字符串为"mykobject02"
    ret = kobject_init_and_add(mykobject02, &mytype, NULL, "%s", "mykobject02");

    return 0;
}

// 模块退出函数
static void mykobj_exit(void)
{
    // 释放mykobject01的引用计数
    kobject_put(mykobject01);

    // 释放mykobject02的引用计数
    kobject_put(mykobject02);
    kset_unregister(mykset);
}

module_init(mykobj_init); // 指定模块的初始化函数
module_exit(mykobj_exit); // 指定模块的退出函数

MODULE_LICENSE("GPL");   // 模块使用的许可证
MODULE_AUTHOR("topeet"); // 模块的作者