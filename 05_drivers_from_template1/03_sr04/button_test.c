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
#include <sys/ioctl.h>

#define CMD_TRIG  100  //表示开始发送超声波，可以为任意值，不建议0或者1，因为已经被占用

static int fd;

/*
 * ./button_test /dev/sr04
 *
 */
int main(int argc, char **argv)
{
	int val;
	struct pollfd fds[1];
	int timeout_ms = 5000;
	int ret;
	int	flags;

	int i;
	
	//如果传入参数小于两个，打印设备使用方法
	if (argc != 2) 
	{
		//打印设备使用方法
		printf("Usage: %s <dev>\n", argv[0]);
		return -1;
	}


	//以阻塞方式打开设备
	fd = open(argv[1], O_RDWR);
	if (fd == -1)  //如果设备打开失败
	{
		//打印错误
		printf("can not open file %s\n", argv[1]);
		//返回错误
		return -1;
	}

	while (1)
	{
		//发送超声波
		ioctl(fd, CMD_TRIG);
		//打印提示，可有可无
		printf("I am goning to read distance: \n");
		//读取超声波的值，将值传入到val中
		if (read(fd, &val, 4) == 4)
			//打印获得的距离，因为内核不能进行除法，所以这里必须自己进行计算
			printf("get distance: %d cm\n", val*17/1000000);
		else
			//如果超声波获取值失败，打印错误
			printf("get distance err\n");
		//因为超声波的两次发送信息必须大于50us，所以给一个1s延时
		sleep(1);
	}

	close(fd);
	
	return 0;
}


