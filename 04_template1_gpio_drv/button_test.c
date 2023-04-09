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
 * ./button_test /dev/100ask_button0
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
	
	//如果输入参数小于2个，打印本程序使用方法
	if (argc != 2) 
	{
		printf("Usage: %s <dev>\n", argv[0]);
		return -1;
	}


    //打开/dev/xxx文件（设备节点），返回一个文件描述符，我们之后可以直接根据这个文件描述符来操作设备节点，间接操作驱动
	//可读可写，非阻塞模式
	fd = open(argv[1], O_RDWR | O_NONBLOCK);
	//如果打开失败
	if (fd == -1)
	{
		printf("can not open file %s\n", argv[1]);
		return -1;
	}

	for (i = 0; i < 10; i++) 
	{
		if (read(fd, &val, 4) == 4)
			printf("get button: 0x%x\n", val);
		else
			printf("get button: -1\n");
	}

	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

	while (1)
	{
		if (read(fd, &val, 4) == 4)
			printf("get button: 0x%x\n", val);
		else
			printf("while get button: -1\n");
	}
	//关闭文件
	close(fd);
	
	return 0;
}


