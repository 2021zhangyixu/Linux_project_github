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



int main(int argc, char **argv)
{
    int fd,ret;    //fd：存放文件描述符；len：存放写入字符个数
    char buf;
    //打开/dev/xxx文件（设备节点），返回一个文件描述符，我们之后可以直接根据这个文件描述符来操作设备节点，间接操作驱动
    fd = open("/dev/hello1", O_RDWR);
	//如果打开失败
    if (fd < 0)
    {
        perror("can not open file /dev/hello1\n");
        return fd;
    }
	/* 作用 ： 将字符串转化为一个整数
	 * argv[1] ：	要转换为长整数的字符串
	 * NULL ：如果提供了 endptr 参数，则将指向解析结束位置的指针存储在 endptr 中。endptr 可以用于进一步处理字符串中的其他内容
	 * 0 ： 设置为 0，则会根据字符串的前缀（如 "0x" 表示十六进制，"0" 表示八进制，没有前缀表示十进制）来自动判断进制
	*/
	buf = 0; 
	printf("Start write\r\n");
	while(1)
	{
		ret = write(fd, &buf, 1);
		//如果写入失败
	    if (ret < 0)
	    {
	        perror("write error\n");
	        return ret;
	    }
	}
	
    //关闭文件
    close(fd);
    return 0;
}
