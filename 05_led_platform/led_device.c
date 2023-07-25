/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释和进行了微调
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是设备代码
 	*4，TAB为4个空格
 * 作者 ： CSDN风正豪
*/

#include <linux/init.h>     //初始化头文件
#include <linux/module.h>   //最基本的文件，支持动态添加和卸载模块。
#include <linux/platform_device.h> //平台设备所需要的头文件

void LED_release(struct device *dev)
{
	printk("LED_device_release \n");
}


// 设备资源信息，也就是LED信息代码
struct resource led_resource[] = {
	{
		.start = 131,
		.end = 131,
		.flags = IORESOURCE_IRQ,
	},
	// {
	// 	.start = 132,
	// 	.end = 132,
	// 	.flags = IORESOURCE_IRQ,
	// 	.name = "LED1",
	// },

};


// platform 设备结构体
struct platform_device led_device = {
	.name = "led_device",  //platform 设备的名字， 用来和 platform 驱动相匹配。 名字相同才能匹配成功
	.id = -1,   // ID 是用来区分如果设备名字相同的时候(通过在后面添加一个数字来代表不同的设备， 因为有时候有这种需求)
	.resource = led_resource,  //指向一个资源结构体数组。 一般包含设备信息
	.num_resources = ARRAY_SIZE(led_resource),  //资源结构体数量
	.dev = {
		.release = LED_release,
		}
};


/* 在入口函数 */
static int __init led_device_init(void)
{
	// 设备信息注册到 Linux 内核
	platform_device_register(&led_device);
	printk("led_device_register ok \n");
	return 0;
}

static void __exit led_device_exit(void)
{
	// 设备信息卸载
	platform_device_unregister(&led_device);
	printk("gooodbye! \n");

}


module_init(led_device_init);  //确认入口函数
module_exit(led_device_exit);  //确认出口函数

/*最后我们需要在驱动中加入 LICENSE 信息和作者信息，其中 LICENSE 是必须添加的，否则的话编译的时候会报错，作者信息可以添加也可以不添加
 *这个协议要求我们代码必须免费开源，Linux遵循GPL协议，他的源代码可以开放使用，那么你写的内核驱动程序也要遵循GPL协议才能使用内核函数
 *因为指定了这个协议，你的代码也需要开放给别人免费使用，同时可以根据这个协议要求很多厂商提供源代码
 *但是很多厂商为了规避这个协议，驱动源代码很简单，复杂的东西放在应用层
*/
MODULE_LICENSE("GPL"); //指定模块为GPL协议
MODULE_AUTHOR("CSDN:qq_63922192");  //表明作者，可以不写



