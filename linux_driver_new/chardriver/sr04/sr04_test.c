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
static u_int64_t buf ;

static void func(int signum)
{
	read(fd,&buf,sizeof(buf));
    printf("app buf is %llu ns \n",buf);
	printf("time is %llu us \n",buf / 1000);
	printf("distance is %f cm \n",(buf/1000)*0.034);
}

int main(int argc, char **argv)
{
	int ret,flags;
	fd = open("/dev/chr_device_sr04", O_RDWR);
	if (fd == -1) {
		printf("can not open file /dev/chr_device_sr04\n");
		return -1;
	}
	// 设置 SIGIO 信号的处理函数为 func，当 I/O 事件触发时调用 func
	signal(SIGIO,func);
	// 设置当前进程为文件描述符 fd 的所有者，以便接收 SIGIO 信号
    fcntl(fd,F_SETOWN,getpid());
	// 获取文件描述符 fd 的当前标志位
    flags = fcntl(fd,F_GETFD);
	// 设置文件描述符 fd 的标志位，启用 FASYNC（异步 I/O 通知）
    fcntl(fd,F_SETFL,flags| FASYNC);

	sleep(1);
	printf("ready\n");
	sleep(1);
	write(fd,&buf,sizeof(buf));
	sleep(3);
	printf("time over\n");

	close(fd);
	
	return 0;
}

