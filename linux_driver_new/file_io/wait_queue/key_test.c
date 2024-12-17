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
#include <signal.h>

static int fd;

int main(int argc, char **argv)
{
	int ret;
	char buf[2];
	fd = open("/dev/wait_queue_device", O_RDWR);
	if (fd == -1)
	{
		printf("can not open file /dev/wait_queue_device\n");
		return -1;
	}
	printf("wait_queue_device read \n");
	read(fd, buf, 2);
	sleep(1);
	printf("wait_queue_device read over \n");
	close(fd);
	
	return 0;
}

