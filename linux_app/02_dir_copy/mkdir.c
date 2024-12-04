#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

// argc:命令行中参数个数
// atgv:命令行中的参数
int main(int argc,char *argv[])
{
	/* 用于存储文件描述符
	 * 0：标准输入（stdin），通常指向键盘输入。
	 * 1：标准输出（stdout），通常指向终端或屏幕。
	 * 2：标准错误（stderr），用于输出错误信息，通常指向终端或屏幕。
	 */
	int fd_src,fd_obj;
	char buf[32] = {0}; // 存储读取到的数据
	char file_path[32] = {0}; // 存储要拷贝的文件路径
	char srcfile_name[32] = {0}; 
	char objfile_name[32] = {0}; 
	ssize_t ret;
	DIR *dp; // 存储文件路径的目录流
	struct dirent *dir; // 文件路径下的文件相关信息结构体

	printf("please enter the file path:\n");
	scanf("%s",file_path);
	
	// 打开指定的文件路径
	dp = opendir(file_path);
	if(dp == NULL){
		printf("opendir is fail\n");
		return -1;
	}
	printf("opendir is ok\n");
	while(1){
		// 读取文件夹中的文件
		dir = readdir(dp);
		if(dir != NULL){
			printf("file name is %s\n",dir->d_name);
		}else{
			break;
		}
		
	}

	printf("please enter the src file name:\n");
	scanf("%s",srcfile_name);
	printf("please enter the obj file name:\n");
	scanf("%s",objfile_name);
	// 1. 以可读可写方式打开源文件，可以理解为双击打开设备
	fd_src = open(strcat(strcat(file_path,"/"),srcfile_name),O_RDWR);
	if(fd_src < 0){
		printf("%s open error",srcfile_name);
		return -1;
	}
	
	/* 以读写方式打开目标文件，当没有目标文件时自动创建
	 * 因为增加了 O_CREAT 选项，因此需要提供 mode 参数来指定文件权限。
	 */
	fd_obj = open(objfile_name,O_RDWR|O_CREAT,0666);
	if(fd_obj < 0){
		printf("%s open error",objfile_name);
		return -1;
	}

	// 2. 此时进行读写操作
	while((ret = read(fd_src,buf,32)) != 0){
		// 作用类似与 printf 将读取到的内容打印到终端
		write(1,buf,ret);
		write(fd_obj,buf,ret);
	}

	/* 3. 释放文件资源
	 * 如果文件描述符没有被关闭，会导致资源泄漏，尤其是在长时间运行的程序中，可能会耗尽可用文件描述符，从而影响程序的稳定性。
	 * 对于某些文件系统，调用close会确保所有未写入的数据被保存到磁盘中，防止数据丢失。
	 */
	close(fd_src);
	close(fd_obj);

	return 0;
}

