#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/*read:
 *write:
 */
int main(int argc,char** argv)
{
    int fd,len;
    char buf[100];
    if(argc < 2)
    {
        printf("Usage:\n");
        printf("%s <dev> [string]\n",argv[0]);
        return -1;
    }

    //open
    fd = open(argv[1],O_RDWR);
    if(fd < 0)
    {
        printf("can not open file %s\n",argv[1]);
        return -1;
    }

    //write
    if(argc == 3)
    {
        len = write(fd,argv[2],strlen(argv[2]+1));
        printf("write ret = %d\n",len);
    }
    //read
    else
    {
        len =read(fd,buf,100);
        buf[99] = '\0';
        printf("read str : %s\n",buf);
    }

    //close
    close(fd);
    return 0;
}

