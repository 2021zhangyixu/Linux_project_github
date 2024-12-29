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

static int fd;


int main(int argc, char **argv)
{
	int ret;
	char buf;

	// if (argc != 2)  {
	// 	printf("Usage: %s <on | off> \n", argv[0]);
	// 	return -1;
	// }

	fd = open("/dev/chr_device_led_pinctrl", O_RDWR);
	if (fd == -1) {
		printf("can not open file /dev/chr_device_led_pinctrl\n");
		return -1;
	}

	// if (strcmp(argv[1], "on") == 0)
	// 	buf = 0;  //因为LED外接3.3V，所以输出低电平才是开灯
	// else
	// 	buf = 1;  //因为LED外接3.3V，所以输出高电平才是关灯
	buf = 0;
	ret = write(fd,&buf, 1);
	sleep(5);
	buf = 1;
	ret = write(fd,&buf, 1);
	sleep(5);
	
	close(fd);
	
	return 0;
}

