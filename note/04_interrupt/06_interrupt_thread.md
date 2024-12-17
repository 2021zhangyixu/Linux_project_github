# 基础概念

1. **当中断处理需要在 `100us` 时间以上时，我们就应当使用下半部**。
2. **中断线程化比工作队列性能开销更小，不支持可延迟工作，灵活性弱通常与中断号绑定。而工作队列支持多队列，多并发等特性。**
3. 无论是**中断线程化**还是**工作队列**，都可能面临多线程环境下的**共享数据访问问题**。所以我们需要合理的进行同步操作，确保数据的一致性和准确性。
4. **部分中断不可以采取中断线程化，例如时钟中断**。有些流氓进程不主动让出处理器，内核只能依靠周期性的时钟中断夺回处理器的控制权，而时钟中断又是调度器的脉搏。因此，对于不可以线程化的中断，我们应当在注册函数的时候设置 `IRQF_NO_THREAD` 标志位。

# API 介绍

1. 对于**不需要延迟工作**的中断下半部，**中断线程化会比工作队列更合适**，因为**工作队列实时性没有中断线程化高**。
2. **中断上半部的处理函数中，返回值必须为 `IRQ_WAKE_THREAD` ！**

| 类型        | 描述                                                         |
| ----------- | ------------------------------------------------------------ |
| 作用        | 将中断号和指定处理函数进行绑定                               |
| `irq`       | 需要注册的中断号，用于标识中断源                             |
| `handler`   | 指向中断处理程序的函数指针，当发生中断之后就会执行该函数，中断上半部 |
| `thread_fn` | 线程化的中断处理函数，及中断下半部；**当中断上半部执行完成后，会返回 `IRQ_WAKE_THREAD` 调度与该下半部相关的内核线程** |
| `flags`     | 中断触发方式及其他配置，常见的标志 : <br /> - `IRQF_SHARED` : 共享中断。**多个设备共享一个中断线**，共享的所有中断都必须指定该标志，并且 `dev` 参数就是用于区分不同设备的唯一标识。<br />- `IRQF_ONESHOT` : **单次中断**，中断只执行一次。<br />- `IRQF_TRIGGER_NONE` : **不依赖外部信号触发**。可能是定时器触发，也可是软件触发。<br />- `IRQF_TRIGGER_RISING` : **上升沿触发**。<br />- `IRQF_TRIGGER_FALLING` : **下降沿触发**。<br />- `IRQF_TRIGGER_HIGH` : **高电平触发**。<br />- `IRQF_TRIGGER_LOW` : **低电平触发**。 |
| `name`      | 中断名称，用于标识中断，我们可以利用 `cat /proc/interrupts` 命令进行查看 |
| `dev_id`    | 如果 `flags` 被设置为了 `IRQF_SHARED`，该参数就是用于区分不同的中断。**该参数也将会被传递给中断处理函数 `irq_handler_t` 的第二个参数**。 |
| 返回值      | 申请成功返回0，否则为负数                                    |

```c
int request_threaded_irq(unsigned int irq,
                         irq_handler_t handler,
                         irq_handler_t thread_fn,
                         unsigned long flags,
                         const char *name,
                         void *dev_id);
```

# 综合示例

```c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

int irq;

irqreturn_t test_work(int irq, void *args)
{
  msleep(1000);
  printk("This is test_work\n");
  return IRQ_RETVAL(IRQ_HANDLED);
}

irqreturn_t test_interrupt(int irq, void *args)
{
  printk("This is test_interrupt\n");
  return IRQ_WAKE_THREAD;
}

static int interrupt_irq_init(void)
{
  int ret;
  irq = gpio_to_irq(112);
  ret = request_threaded_irq(irq, test_interrupt, test_work, IRQF_TRIGGER_RISING, "test", NULL);
  if (ret < 0)
  {
    printk("request_irq is error\n");
    return -1;
  }

  return 0;
}

static void interrupt_irq_exit(void)
{
  free_irq(irq, NULL);
  printk("bye bye\n");
}

module_init(interrupt_irq_init);
module_exit(interrupt_irq_exit);

MODULE_LICENSE("GPL");
```

