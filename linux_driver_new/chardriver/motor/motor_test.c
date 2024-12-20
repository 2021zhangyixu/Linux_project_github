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


/*
 * ./button_test -100  1
 *
 */
int main(int argc, char **argv)
{
	int fd,ret;
	int buf[2];

	if (argc != 3) 
	{
		printf("Usage: %s <step_number> <mdelay_number>\n", argv[0]);
		return -1;
	}

	fd = open("/dev/chr_device_motor", O_RDWR);
	if (fd == -1) {
		printf("can not open file /dev/chr_device_motor\n");
		return -1;
	}
	buf[0] = strtol(argv[1], NULL, 0);
	buf[1] = strtol(argv[2], NULL, 0);

	ret = write(fd, buf, 8);
	close(fd);
	
	return 0;
}

