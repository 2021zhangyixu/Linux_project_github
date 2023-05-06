/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释。
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是应用层代码
 * 作者 ： CSDN风正豪
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>

static int fd;

/*
 * ./button_test /dev/mydht11
 *
 */
int main(int argc, char **argv)
{
	char buf[2];
	int ret;

	int i;
	
	//如果传入参数不等于两个，打印设备使用方法
	if (argc != 2) 
	{
		printf("Usage: %s <dev>\n", argv[0]);
		return -1;
	}

	//以非阻塞方式打开设备
	fd = open(argv[1], O_RDWR | O_NONBLOCK);
	//如果设备打开失败
	if (fd == -1)
	{
		//打印错误
		printf("can not open file %s\n", argv[1]);
		//返回错误
		return -1;
	}

	while (1)
	{
		if (read(fd, buf, 2) == 2)
			printf("get Humidity: %d, Temperature : %d\n", buf[0], buf[1]); //打印湿度和温度
		else
			printf("get dht11: -1\n");

		sleep(5);
	}

	//sleep(30);

	close(fd);
	
	return 0;
}


