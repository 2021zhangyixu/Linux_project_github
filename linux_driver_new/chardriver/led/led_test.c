/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释。
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是应用层代码
 	*4，TAB为4个空格
 * 作者 ： CSDN风正豪
*/

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>

static int fd;


//int led_on(int which);
//int led_off(int which);
//int led_status(int which);

/* 可执行文件名   | 表示要操作哪一盏灯  | 灯状态  |    效果
 * ./led_test    |   <0|1|2|..>        | on     |硬件上开灯
 * ./led_test    |   <0|1|2|..>        | off    |硬件上关灯
 * ./led_test    |   <0|1|2|..>        |        |读取led状态，并且显示在终端
 */
int main(int argc, char **argv)
{
	int ret;     //存放函数返回值，用于判断函数是否正常执行
	char buf[2]; //存放命令行的后两个字符（<0|1|2|...> [on | off]）

	
	//如果传入参数少于两个，打印文件用法
	if (argc < 2) 
	{
		printf("Usage: %s <0|1|2|...> [on | off]\n", argv[0]);
		return -1;
	}


	//打开文件，因为在驱动层中，device_create()函数创建的设备节点名字叫做100ask_led，而设备节点都存放在/dev目录下，所以这里是/dev/100ask_led
	fd = open("/dev/chr_device_led", O_RDWR);
	//如果无法打开，返回错误
	if (fd == -1)
	{
		printf("can not open file /dev/chr_device_led\n");
		return -1;
	}
	//如果传入了三个参数，表示写入
	if (argc == 3)
	{
		printf("write\n");
		/* write */
		/* 作用 ： 将字符串转化为一个整数
		 * argv[1] ：  要转换为长整数的字符串
		 * NULL ：如果提供了 endptr 参数，则将指向解析结束位置的指针存储在 endptr 中。endptr 可以用于进一步处理字符串中的其他内容
		 * 0 ： 设置为 0，则会根据字符串的前缀（如 "0x" 表示十六进制，"0" 表示八进制，没有前缀表示十进制）来自动判断进制
		*/
		buf[0] = strtol(argv[1], NULL, 0);

		//判断是否为打开
		if (strcmp(argv[2], "on") == 0)
			buf[1] = 0;  //因为LED外接3.3V，所以输出低电平才是开灯
		else
			buf[1] = 1;  //因为LED外接3.3V，所以输出高电平才是关灯
		//向字符驱动程序中写入
		ret = write(fd, buf, 2);
	}
	//否则表示读取电平信息
	else
	{
		printf("read\n");
		/* read */
		/* 作用 ： 将字符串转化为一个整数
		 * argv[1] ：  要转换为长整数的字符串
		 * NULL ：指向第一个不可转换的字符位置的指针
		 * 0 ： 表示默认采用 10 进制转换
		*/
		buf[0] = strtol(argv[1], NULL, 0);
		//读取电平，从驱动层读取两个数据
		ret = read(fd, buf, 2);
		//如果返回值为2，表示正常读取到了电平。（为什么是2，看驱动程序的gpio_drv_read）
		if (ret == 0)
		{
			//打印引脚信息
			printf("led %d status is %s\n", buf[0], buf[1] == 0 ? "on" : "off");
		}
	}
	
	close(fd);
	
	return 0;
}

