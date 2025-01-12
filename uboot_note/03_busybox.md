# busybox 移植

## 中文支持

1. 如果编译默认的 busybox ，那么中文字符将会显示不正常，因此需要在 `libbb/printable_string.c` 中注释如下四行代码。

```c
const char* FAST_FUNC printable_string(uni_stat_t *stats, const char *str)
{
		// if (c >= 0x7f)
		// 	break;

    //...

			// if (c < ' ' || c >= 0x7f)
			// 	*d = '?';

}
```

2. `libbb/unicode.c`  对如下内容进行修改，主要是禁止字符大于 0x7f 的时候设置为 '?'。

```c
// *d++ = (c >= ' ' && c < 0x7f) ? c : '?';
*d++ = (c >= ' ') ? c : '?';

/* if (c < ' ' || c >= 0x7f) */
if (c < ' ')
```

## 配置 busybox

1. 采用动态编译：` Settings -> Command line editing ->` 
2. 选中 ` Settings -> Command line editing -> vi-style line editing commands`  。
3. `Linux System Utilities -> mdev ` 下面的所有内容选中。
4.  ` Settings -> Support Unicode` 和下面的一个选项进行选中
5. 编译

```shell
make install CONFIG_PREFIX=/home/book/Desktop/samba/busybox
```

6. 检验

```shell
$/home/book/Desktop/samba/busybox: ls
bin  linuxrc  sbin  usr
```

## 移植 lib 文件夹

### 移植 arm-linux-gnueabihf/libc/lib 文件

1. 在 `/home/book/Desktop/samba/busybox` 路径中创建一个 lib 文件夹

```shell
mkdir lib
```

2. 进入 `${gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf}/arm-linux-gnueabihf/libc/lib` 目录，执行如下命令。

```shell
cd ${gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf}/arm-linux-gnueabihf/libc/lib
cp *so* *.a /home/book/Desktop/samba/busybox/lib -d
```

3. 此时的 `ld-linux-armhf.so.3` 是一个符号链接。

```shell
cd /home/book/Desktop/samba/busybox/lib
ls ld-linux-armhf.so.3 -l
rm ld-linux-armhf.so.3
```

4. 重新将 `ld-linux-armhf.so.3` 拷贝过来。

```shell
cd ${gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf}/arm-linux-gnueabihf/libc/lib
cp ld-linux-armhf.so.3 /home/book/Desktop/samba/busybox/lib
```

5. 检查文件大小，从大小上我们可以知道，拷贝成功。

```shell
$ ls ld-linux-armhf.so.3 -l
-rwxr-xr-x 1 book book 724392 Jan  9 02:57 ld-linux-armhf.so.3
```

### 移植 arm-linux-gnueabihf/lib 文件

1. 将 `${gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf}/arm-linux-gnueabihf/lib` 中的库也进行拷贝。

```shell
cd ${gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf}/arm-linux-gnueabihf/lib
cp *so* *.a /home/book/Desktop/samba/busybox/lib -d
```

2. 进入 `${busybox}/usr` 目录创建 lib 文件夹。

```shell
cd /home/book/Desktop/samba/busybox/usr
mkdir lib
```

3. 同理将如下目录中的 `*so* *.a` 文件拷贝过来

```shell
cd ${gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf}/arm-linux-gnueabihf/libc/usr/lib
cp *so* *.a /home/book/Desktop/samba/busybox/lib -d
```

4. 最终查看文件大小

```shell
$ cd /home/book/Desktop/samba/busybox
$ du lib usr/lib -sh
57M	lib
67M	usr/lib
```

### 添加相关文件夹

1. 创建其他文件夹

```shell
mkdir mnt sys tmp root dev proc
```

2. 创建 `/dev/tty2` 文件

```shell
mkdir dev
touch tty2
```

## 初步测试

1. 使用 NFS 进行挂载根文件系统。

```shell
# 原来的环境变量
bootargs=console=ttymxc0,115200 root=/dev/mmcblk1p2
# 现在设置新的环境变量
setenv bootargs 'console=ttymxc0,115200 root=/dev/nfs nfsroot=192.168.5.11:/home/book/Desktop/samba/busybox,proto=tcp rw  ip=192.168.5.9:192.168.5.11:192.168.5.1:255.255.255.0::eth0:off'
saveenv
```

2. 重启开发板

```shell
tftp 80800000 zImage
tftp 83000000 imx6ull-sailing-14x14-emmc.dtb
bootz 80800000 - 83000000 
```

3. 如果希望 uboot 默认为这个值，应该在

```shell
CONFIG_BOOTARGS 
```



## 进一步完善 busybox

### 创建 `/etc/init.d/rcS`  文件

1. 在上面的测试完成后，我们能够看到如下打印。

```shell
can't run '/etc/init.d/rcS': No such file or directory
```

2. 创建 `/etc/init.d/rcS`  文件。`rcS` 是一个 shell 脚本，Linux 内核启动以后需要启动一些服务，而 rcS 就是用来规定启动哪些文件的脚本文件。

```shell
mkdir etc
cd etc
mkdir init.d
cd init.d
vi rcS
# 加入如下内容
# ----------------------------------------------------
#!/bin/sh
PATH=/sbin:/bin:/usr/sbin:/usr/bin:$PATH
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/lib:/usr/lib
export PATH LD_LIBRARY_PATH
mount -a
mkdir /dev/pts
mount -t devpts devpts /dev/pts
echo /sbin/mdev > /proc/sys/kernel/hotplug
mdev -s
# ----------------------------------------------------
chmod 777 rcS
```

3. 重启开发板，此时上面那一条打印信息消失，变成如下打印。

```shell
can't read '/etc/fstab': No such file or directory
```

### 创建 `/etc/fstab`

1. fstab 在 Linux 开机以后自当配置哪些需要自动挂载的分区。

```shell
cd etc
vi fstab
# 加入如下内容
# ----------------------------------------------------
#<file system> <mount point> <type> <options> <dump> <pass>
proc           /proc         proc   defaults  0      0
tmpfs          /tmp          tmpfs  defaults  0      0
sysfs          /sys          sysfs  defaults  0      0
# ----------------------------------------------------
```

### 创建 `/etc/inittab`

1. init 进程会根据 `/etc/inittab` 这个文件来在不同的运行级别启动相应的进程或执行相应操作。

```shell
cd etc
vi inittab
# 加入如下内容
# ----------------------------------------------------
#etc/inittab
::sysinit:/etc/init.d/rcS
console::askfirst:-/bin/sh
::restart:/sbin/init
::ctrlaltdel:/sbin/reboot
::shutdown:/bin/umount -a -r
::shutdown:/sbin/swapoff -a
# ----------------------------------------------------
```

### 创建 `/etc/resolv.conf`

1. 如果我们希望连接外网而非局域网，那么需要利用 `/etc/resolv.conf` 文件进行 DNS 解析。

```shell
 vi /etc/resolv.conf

# 加入如下内容
# ----------------------------------------------------
# 运营商的域名系欸服务器地址
nameserver 114.114.114.114
# 所处网段的网关地址
nameserver 192.168.1.1
# ----------------------------------------------------
```

## 最终测试

1. 在 Ubuntu 中使用交叉编译工具链，编译一个测试程序，然后在开发板中运行。

```shell
vim hello.c     
# 加入如下内容
# ----------------------------------------------------
#include <stdio.h>

int main()
{
	while(1){
		printf("sailing hello world\n");
		sleep(2);
	}
	return 0;
}
# ----------------------------------------------------
arm-linux-gnueabihf-gcc hello.c 
```

2. 在开发板中创建一个 中文测试 文件夹。

```shell
mkdir 中文测试
```

# 设置开机自启动

1. 修改  `/etc/init.d/rcS`  文件即可。

```shell
#!/bin/sh
PATH=/sbin:/bin:/usr/sbin:/usr/bin:$PATH
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/lib:/usr/lib
export PATH LD_LIBRARY_PATH
mount -a
mkdir /dev/pts
mount -t devpts devpts /dev/pts
echo /sbin/mdev > /proc/sys/kernel/hotplug
mdev -s
# self-starting
./a.out &
```

# 打包根文件系统

1. 

```shell
cd ${busybox_PATH}
tar -vcjf rootfs.tar.bz2 *
```





# 问题

## NFS 版本问题

1. U-Boot 通常使用 NFSv2 协议进行网络文件系统操作。 然而，具体的 NFS 版本信息可能不会直接显示在 U-Boot 的命令中。 因此，您需要确保服务器端（例如 Ubuntu 系统）支持 NFSv2，以确保与 U-Boot 的兼容性。

2. 在 Ubuntu 系统中，您可以通过以下命令查看 NFS 服务器支持的版本：

```shell
$ cat /proc/fs/nfsd/versions
# + 表示支持的 NFS 版本，- 表示不支持的版本。-2 表示 NFSv2 未启用，+3、+4 等表示 NFSv3、NFSv4 等已启用。
-2 +3 +4 +4.1 +4.2
```

3. 如果发现不支持 NFSv2 那么需要修改如下文件

```shell
sudo vim /etc/default/nfs-kernel-server
# 在上述文件中添加这一行内容
RPCNFSDOPTS="--nfs-version 2,3,4"
# 重启 NFS 服务
sudo systemctl restart nfs-kernel-server
```

## 开机自启动后台运行问题

1. 当我们修改  `/etc/init.d/rcS`  文件设置了开机自启动之后，如果文件未将其置于后台运行（即未使用 `&`），可能会导致控制台失去响应。这是因为 `rcS` 脚本在执行过程中，**若调用的程序未在后台运行，`rcS` 会等待该程序执行完毕后再继续**。