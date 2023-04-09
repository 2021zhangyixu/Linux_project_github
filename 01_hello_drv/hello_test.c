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

/* 作用：输入三个参数表示向/dev/xxx文件中写入数据，输入非三个数据，表示向/dev/xxx文件中读取数据。
 * 写 : ./hello_test /dev/xxx 100ask
 * 读 : ./hello_test /dev/xxx
 */
int main(int argc,char** argv)
{
    int fd,len;    //fd：存放文件描述符；len：存放写入字符个数
    char buf[100]; //存放文件中的字符
	//如果输入参数小于2个，打印本程序使用方法
    if(argc < 2)
    {
        printf("Usage:\n");
        printf("%s <dev> [string]\n",argv[0]);
        return -1;
    }

    //打开/dev/xxx文件（设备节点），返回一个文件描述符，我们之后可以直接根据这个文件描述符来操作设备节点，间接操作驱动
    fd = open(argv[1],O_RDWR);
	//如果打开失败
    if(fd < 0)
    {
        printf("can not open file %s\n",argv[1]);
        return -1;
    }

    //如果输入参数为3个，表示写入数据
    if(argc == 3)
    {
		//因为strlen计算字符串长度不会包括'\0'，所以需要+1
        len = write(fd,argv[2],strlen(argv[2])+1);
		//打印出写入字符个数
        printf("write ret = %d\n",len);
    }
    //否则为读取数据
    else
    {
		//读入100个字符
        len =read(fd,buf,100);
		//无论传入多少个数据，都只会读100个字符
        buf[99] = '\0';
		//打印读取到的字符
        printf("read str : %s\n",buf);
    }

    //关闭文件
    close(fd);
    return 0;
}

