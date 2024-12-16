# 文件操作

## 应用层和驱动层对应关系

| 应用层 | 驱动层的 file_operation 结构体成员 |
| ------ | ---------------------------------- |
| read   | read                               |
| write  | write                              |
| ioctl  | unlocked                           |
| open   | open                               |
| close  | release                            |

## 内核空间与用户空间的数据交换

1. 内核空间中的代码控制了硬件资源，**用户空间中的代码只能通过内核暴露的系统调用接口来使用系统中的硬件资源**，这样的设计可以保证操作系统自身的安全性和稳定性。
2. 在 32 位 Linux 系统中，整个虚拟地址空间为 4GB，通常划分：
   - **用户空间**: 从地址 0x00000000 到 0xBFFFFFFF（共3GB），用于运行用户进程和应用程序。
   - **内核空间**: 从地址 0xC0000000 到 0xFFFFFFFF（共1GB），用于操作系统内核和驱动程序。
3. **进程只有从用户空间切换到内核空间才可以使用系统的硬件资源**，切换的方式有三种：**系统调用**，**软中断**，**硬中断**。
4. **内核空间和用户空间的内存是不能互相访问的**，因此我们需要利用 `copy_to_user()` 函数把内核空间的数据复制到用户空间，利用 `copy_from_user()` 函数把用户空间的数据复制到内核空间。

```c
/* 作用 : 将数据从内核空间复制到用户空间
 * to   : 目标地址，指向用户空间的缓冲区
 * from : 源地址，指向内核空间的缓冲区
 * n    : 要复制的字节数
 * 返回值 : 该函数返回未能复制的字节数。如果成功复制所有数据，则返回值为0。如果发生错误（例如，目标地址不可访问），则返回相应的字节数
 */
unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);

/* 作用 : 从用户空间复制到内核空间
 * to   : 目标地址，指向内核空间的缓冲区
 * from : 源地址，指向用户空间的缓冲区
 * n    : 要复制的字节数
 * 返回值 : 该函数同样返回未能复制的字节数。如果成功复制所有数据，则返回值为0。如果发生错误（例如，源地址不可访问），则返回相应的字节数
 */
unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
```

5. **注：当应用层调用 `read()` 函数时，我们也可以给驱动层传递数据，但不建议这么做！**

```c
/******* 应用层 *******/
// buf[0] 为传递给驱动层的数据，buf[1] 为驱动层返回的数据
char buf[2];
buf[0] = strtol(argv[1], NULL, 0);
ret = read(fd, buf, 2);

/******* 驱动层 *******/
// 查看应用层希望获得那个 GPIO 的电平信息
ret = copy_from_user(tmp_buf, buf, 1);
// 读取电平
tmp_buf[1] = gpio_get_value(gpios[(int)tmp_buf[0]].gpio);
// 将读取到的数据返回给应用层
ret = copy_to_user(buf, tmp_buf, 2);
```

## IO 模型

1. IO 模型根据实现的功能可以划分为为**阻塞 IO**、**非阻塞 IO**、**信号驱动 IO**， **IO 多路复用**和**异步 IO**。根据等待 IO 的执行结果进行划分，**前四个 IO 模型又被称为同步 IO**。
2. 所谓同步，即发出一个功能调用后，只有得到结果该调用才会返回。
   - **非阻塞 IO** : 非阻塞 IO 进行 IO 操作时，如果内核数据没有准备好，内核会立即向进程返回 err，不会进行阻塞。非阻塞 IO 的优点是**效率高**，同样的时间可以做更多的事。但是缺点也很明显，**需要不断对结果进行轮询查看**，从而导致结果获取不及时。
   - **IO 多路复用** : 通常情况下使用 `select()、poll()、epoll()` 函数实现 IO 多路复用。
   - **信号驱动 IO** : 系统在一些事件发生之后，会**对进程发出特定的信号**，而信号与处理函数相绑定，**当信号产生时就会调用绑定的处理函数**。例如在 Linux 系统任务执行的过程中可以按下 `Ctrl+C` 来对任务进行终止，系统实际上是对该进程发送一个 `SIGINT` 信号，该信号的默认处理函数就是退出当前程序。
   - **异步 IO** : aio_read 函数常常用于异步 IO，当进程使用 aio_read 读取数据时，如果数据尚未准备就绪就立即返回，不会阻塞.
3. 当一个异步过程调用发出后，调用者并不能立刻得到结果，实际处理这个调用的部件在完成后，通过状态、通知和回调来通知调用者。

### 阻塞IO

1. 进程在操作 IO 时，首先先会发起一个系统调用，从而转到内核空间进行处理，内核空间的数据没有准备就绪时，进程会被阻塞，不会继续向下执行，直到内核空间的数据准备完成后，数据才会从内核空间拷贝到用户空间，最后返回用户进程。**比较有代表性的是 C 语言中的 `scanf()` 函数。**

2. **阻塞 IO 在 Linux 内核中是非常常用的 IO 模型，所依赖的机制是等待队列**。

3. 等待队列分为**等待队列头**和**等待队列项**。**等待队列项可以是空的，只是用一个单独的头部也可以表示一个等待队列**。

![waitqueue](.\img\waitqueue.png)

#### 初始化等待队列

初始化等待队列有如下两种方法，一般我们使用 `DECLARE_WAIT_QUEUE_HEAD` 宏方式进行初始化等待队列。

```c
/* 作用 : 初始化一个等待队列头
 * q : 等待队列头部，指向一个 wait_queue_head_t 类型的变量，该变量用于表示一个等待队列
 */
void init_waitqueue_head(wait_queue_head_t *q);

/******* 示例 *******/
wait_queue_head_t my_waitqueue;
init_waitqueue_head(&my_waitqueue);

/* 作用 : 初始化一个等待队列头
 * name : ：等待队列头的名称，即你要声明的 wait_queue_head_t 变量名
 */
DECLARE_WAIT_QUEUE_HEAD(name);

/******* 示例 *******/
DECLARE_WAIT_QUEUE_HEAD(my_waitqueue);
```

#### 创建并添加等待队列项

```c
/* 作用 : 用于定义和初始化一个等待队列项
 * name : 等待队列项的变量名，它是 wait_queue_t 类型
 * task : 等待队列项对应的线程或进程（通常是 current，即当前线程或进程）
 */
DECLARE_WAITQUEUE(name, task); 

/* 作用 : 将一个等待队列项（wait_queue_t）添加到一个等待队列头（wait_queue_head_t）中
 * q    : 指向一个等待队列头的指针，表示线程将被添加到这个队列中
 * wait : 指向一个等待队列项（wait_queue_t）的指针，表示需要加入队列的具体线程或进程
 */
void add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait);

/* 作用 : 从等待队列中移除一个等待队列项（wait_queue_t）
 * q    : 指向等待队列头的指针，表示该线程将被从这个队列中移除
 * wait : 指向等待队列项的指针，表示需要移除的队列项（通常是一个等待的线程或进程）
 */
void remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait);

/******* 示例 *******/
static DECLARE_WAIT_QUEUE_HEAD(my_waitqueue);  // 声明并初始化等待队列头
static int my_thread_fn(void *data)
{
    wait_queue_t my_waitqueue_item;

    DECLARE_WAITQUEUE(my_waitqueue_item, current);  // 声明并初始化等待队列项
    add_wait_queue(&my_waitqueue, &my_waitqueue_item);  // 将等待队列项添加到队列
    // ...
    remove_wait_queue(&my_waitqueue, &my_waitqueue_item);  // 将等待队列项从队列删除
}
```

#### 等待事件

```c
/* 作用 : 让线程阻塞，直到给定的条件为 true 时才继续执行，不会中断
 * q         : 等待队列头，指向 wait_queue_head_t 类型的变量，表示等待队列
 * condition : 线程等待的条件，条件为 true 时，线程会被唤醒并继续执行
 */
wait_event(q, condition);

/* 作用 : 允许等待期间被信号中断，适用于那些需要处理中断的场景
 * q         : 等待队列头，指向 wait_queue_head_t 类型的变量
 * condition : 等待的条件，当条件满足时线程继续执行
 */
wait_event_interruptible(q, condition);

/* 作用 : 让线程在给定的时间内等待某个条件满足.如果在超时之前条件被满足，线程会被唤醒;如果超时，线程会在超时后继续执行
 * q         : 等待队列头，指向 wait_queue_head_t 类型的变量
 * condition : 等待的条件，线程会等待直到该条件为真
 * timeout   : 超时时间，表示线程最多等待多长时间;如果超时仍未满足条件，线程会继续执行
 */
wait_event_timeout(q, condition, timeout);

/* 作用 : 结合了 wait_event_interruptible 和 wait_event_timeout 的功能，线程可以在超时之前等待条件的变化，并且能够响应中断信号
 * q         : 等待队列头，指向 wait_queue_head_t 类型的变量
 * condition : 等待的条件
 * timeout   : 超时时间
 */
wait_event_interruptible_timeout(q, condition, timeout);

/* 作用 : 与 wait_event_interruptible 类似，但它通过某种机制（例如锁）确保只有一个线程能够同时进入该等待队列操作
 * q         : 等待队列头，指向 wait_queue_head_t 类型的变量
 * condition : 等待的条件
 */
wait_event_interruptible_exclusive(q, condition);
```

#### 等待队列唤醒

```c
/* 作用 : 用于唤醒等待队列中的所有进程，无论这些进程是否处于可中断的睡眠状态
 * q : 指向等待队列头的指针（wait_queue_head_t 类型），表示需要唤醒的等待队列
 */
void wake_up(wait_queue_head_t *q);

/* 作用 : 只唤醒处于 可中断睡眠状态 的进程
 * q : 指向等待队列头的指针（wait_queue_head_t 类型），表示需要唤醒的等待队列
 */
void wake_up_interruptible(wait_queue_head_t *q);
```

## 信号驱动 IO

1. 信号驱动 IO 无需应用程序查询设备状态，一旦设备准备就绪，就会触发 `SIGIO` 信号，进而调用注册的函数。

### 应用程序

1. 实现信号驱动 IO ，应用程序需要完成如下三步。
   - 注册信号处理函数。应用程序通过 `signal()` 函数注册 `SIGIO` 信号处理函数。
   - 指定接收 `SIGIO` 信号的进程。
   - 开启信号驱动 IO，利用 `F_GETFD` 标志位。

```c
static void func(int signum)
{
	read(fd,&buf,sizeof(buf));
    printf("app buf is %llu ns \n",buf);
    // ...
}

int main(int argc, char **argv)
{
	int ret,flags;
	fd = open("/dev/chr_device_sr04", O_RDWR);
	if (fd == -1) {
		printf("can not open file /dev/chr_device_sr04\n");
		return -1;
	}
	// 1. 设置 SIGIO 信号的处理函数为 func，当 I/O 事件触发时调用 func
	signal(SIGIO,func);
	// 2. 设置当前进程为文件描述符 fd 的所有者，以便接收 SIGIO 信号
    fcntl(fd,F_SETOWN,getpid());
	// 获取文件描述符 fd 的当前标志位
    flags = fcntl(fd,F_GETFD);
	// 3. 设置文件描述符 fd 的标志位，启用 FASYNC（异步 I/O 通知）
    fcntl(fd,F_SETFL,flags| FASYNC);
	
    // ...
	close(fd);
	
	return 0;
}
```

### 驱动程序

1. 在 `file_operations` 结构体中实现 `fasync` 成员函数。并且在 `fasync` 成员函数中调用 `fasync_helper()` 函数来操作 `fasync_struct` 结构体。

```c
typedef struct chr_drv {
	// ...
    struct fasync_struct *fasync;
}chr_drv;


static int chrdev_fasync(int fd, struct file *file, int on)
{
    chr_drv *chrdev_private_data = (chr_drv *)file->private_data;
    return fasync_helper(fd,file,on,&chrdev_private_data->fasync);
}

static struct file_operations chr_file_operations = {
	.owner = THIS_MODULE,
    .fasync = chrdev_fasync, 
    // ...
};
```

2. 当驱动中数据准备好了以后，就可以利用 `kill_fasync()` 函数来通知应用程序。例如我在一个中断中得到了数据，然后调用该函数。此时应用程序中 `signal()` 函数注册的函数将会被调用，在这个被调用的函数中，我们可以使用 `read()` 函数进行读取要获取的数据。

```c
static irqreturn_t echo_isr_fun(int irq, void *dev_id)
{
    // ...
    kill_fasync(&s_char_drv.fasync,SIGIO,POLL_IN);    
    return IRQ_HANDLED;
}
```



## IOCTL 驱动传参

应用程序通过向内核空间写入 1 和 0 从而控制 GPIO 引脚的亮灭，但是**读写操作主要是数据流对数据进行操作**，而一些复杂的控制通常需要非数据操作，这个时候就需要使用 `ioctl` 函数进行实现。



