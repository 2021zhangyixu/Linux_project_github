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

	fd = open("/dev/chr_device_key", O_RDWR);
	if (fd == -1)
	{
		printf("can not open file /dev/chr_device_key\n");
		return -1;
	}
	printf("please press key\n");
	sleep(10);
	printf("time over\n");
	close(fd);
	
	return 0;
}

