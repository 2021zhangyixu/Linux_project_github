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
#include <stdlib.h>

static int fd;

/*
 * ./button_test /dev/motor -100  1
 *
 */
int main(int argc, char **argv)
{
	int buf[2];
	int ret;
	
	//如果传入参数小于两个，打印设备使用方法
	if (argc != 4) 
	{
		//dev表示设备节点，step_number步进次数，mdelay_number每步进一次要延时多长时间
		printf("Usage: %s <dev> <step_number> <mdelay_number>\n", argv[0]);
		return -1;
	}


	//以非阻塞方式打开设备
	fd = open(argv[1], O_RDWR | O_NONBLOCK);
	if (fd == -1)
	{
		//打印错误
		printf("can not open file %s\n", argv[1]);
		//返回错误
		return -1;
	}

	buf[0] = strtol(argv[2], NULL, 0); //将step_number从字符串转换为整数
	buf[1] = strtol(argv[3], NULL, 0); //将mdelay_number从字符串转换为整数

	//让
	ret = write(fd, buf, 8);
	close(fd);
	
	return 0;
}


