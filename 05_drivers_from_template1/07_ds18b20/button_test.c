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
 * ./button_test /dev/myds18b20
 *
 */
int main(int argc, char **argv)
{
	int buf[2];
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
		if (read(fd, buf, 8) == 8)
			printf("get ds18b20: %d.%d\n", buf[0], buf[1]);
		else
			printf("get ds18b20: -1\n");
		
		sleep(5);
	}
	
	close(fd);
	
	return 0;
}


