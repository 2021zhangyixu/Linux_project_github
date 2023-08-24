/* 说明 ： 
 	*1，本代码是学习韦东山老师的驱动入门视频所写，增加了注释。
 	*2，采用的是UTF-8编码格式，如果注释是乱码，需要改一下。
 	*3，这是应用层代码
 	*4，TAB为4个空格
 * 作者 ： CSDN风正豪
*/


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum status{
	OFF = 0,
	ON,
};


int main(int argc, char **argv)
{
    int key_fd,waitqueue_fd,ret;    //fd：存放文件描述符；len：存放写入字符个数
    char buf2[32]="hello";
	char key_buf[2]; //存放驱动层发过来的数据
	
	key_fd = open("/dev/100ask_key", O_RDWR);
	if (key_fd == -1)	//如果无法打开，返回错误
	{
		printf("can not open file /dev/100ask_key\n");
		return key_fd;
	}
    //打开/dev/xxx文件（设备节点），返回一个文件描述符，我们之后可以直接根据这个文件描述符来操作设备节点，间接操作驱动
	waitqueue_fd = open("/dev/hello1", O_RDWR | O_NONBLOCK);
	//如果打开失败
	if (waitqueue_fd < 0)
	{
		perror("can not open file /dev/hello1\n");
		return waitqueue_fd;
	}

	while(1)
	{
		ret = read(key_fd, key_buf, 2);
		if(ret == 2) //如果返回的是正常数据
		{
			if(key_buf[0] == ON) //如果是key1被按下
			{
				printf("write before\r\n");
				ret = write(waitqueue_fd, buf2, strlen(buf2));
				printf("write after\r\n");
				while(key_buf[0] == ON)
				{
					ret = read(key_fd, key_buf, 2);
					if(ret != 2) //如果返回的非正常数据
					{
						return -1;
					}
				}
			}
//			if(key_buf[1] == ON) //如果是key2被按下,关灯
//			{
//				led_buf[1] = OFF;  
//				ret = write(led_fd, led_buf, 2);
//				ret = read(led_fd, led_buf, 2);
//				printf("LED is %s\r\n",led_buf[1] == ON ? "ON" : "OFF");
//			}
		}
		else
		{
			printf("key read error!/r/n");
		}
	}
	
    //关闭文件
    close(waitqueue_fd);
    close(key_fd);
    return 0;
}
