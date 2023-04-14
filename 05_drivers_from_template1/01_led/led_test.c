
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

/* 可执行文件名        | 表示要操作哪一盏灯        | 灯状态 |    效果
 * ./led_test    |   <0|1|2|..>      | on     |开灯
 * ./led_test    |   <0|1|2|..>      | off    |关灯
 * ./led_test    |   <0|1|2|..>      |        |读取led状态
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


	//打开文件
	fd = open("/dev/100ask_led", O_RDWR);
	//如果无法打开，返回错误
	if (fd == -1)
	{
		printf("can not open file /dev/100ask_led\n");
		return -1;
	}
	//如果传入了三个参数，表示写入
	if (argc == 3)
	{
		/* write */
		/* 作用 ： 将字符串转化为一个整数
		 * argv[1] ：  要转换为长整数的字符串
		 * NULL ：指向第一个不可转换的字符位置的指针
		 * 0 ： 表示默认采用 10 进制转换
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
		/* read */
		/* 作用 ： 将字符串转化为一个整数
		 * argv[1] ：  要转换为长整数的字符串
		 * NULL ：指向第一个不可转换的字符位置的指针
		 * 0 ： 表示默认采用 10 进制转换
		*/
		buf[0] = strtol(argv[1], NULL, 0);
		//读取电平
		ret = read(fd, buf, 2);
		//如果返回值为2，表示正常读取到了电平。（为什么是2，看驱动程序的gpio_drv_read）
		if (ret == 2)
		{
			//打印引脚信息
			printf("led %d status is %s\n", buf[0], buf[1] == 0 ? "on" : "off");
		}
	}
	
	close(fd);
	
	return 0;
}


