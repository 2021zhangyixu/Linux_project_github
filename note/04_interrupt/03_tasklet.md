# 基础概念

1. `Tasklets` 是一种基于**软中断（`Softirqs`）**的特殊机制，被广泛应用于中断下半部。
2. 它**在多核处理器上可以避免并发问题，因为 `Tasklet` 会绑定一个 `CPU` 执行**。
3. `Tasklet` 绑定的中断函数**不可以调用可能导致休眠的函数**，例如 `msleep()` ，否则会引起内核异常。
4. 虽然**不可以调用引起休眠的函数**，但**可以调用** `mdelay()` 通过消耗 `CPU` 实现的**纯阻塞延时函数**。不过依旧不建议在中断中调用延时函数，因为**中断要求快进快出**。
5. `tasklet` 可以静态初始化，也可以动态初始化。如果**使用静态初始化，那么就无法动态销毁**。因此在不需要使用 `tasklet` 时，应当避免使用此方法。

# API 介绍

## 静态初始化

1. 在 `Linux` 内核中存在一个静态初始化 `tasklet` 的宏函数，初始状态为使能。

| 类型   | 描述                                 |
| ------ | ------------------------------------ |
| 作用   | 静态初始化 `tasklet`，初始状态为使能 |
| `name` | `tasklet` 名称，用于标识当前中断     |
| `func` | 处理函数                             |
| `data` | 传递给处理函数的参数                 |

```c
#define DECLARE_TASKLET(name, func, data) \
struct tasklet_struct name = { NULL, 0, ATOMIC_INIT(0), func, data }
```

2. 如果希望初始状态为非使能，那么就可以使用下面这个宏。如果是通过如下宏进行的初始化，**使用 `tasklet` 之前要先调用 `tasklet_enable` 使能**。

```c
#define DECLARE_TASKLET_DISABLED(name, func, data) \
struct tasklet_struct name = { NULL, 0, ATOMIC_INIT(1), func, data }
```

## 动态初始化和销毁

1. `tasklet_init()` 可以对 `tasklet` 动态初始化，为软中断准备好响应的下半部处理机制，并不会触发中断。
2. `request_irq()` 注册硬件中断处理程序，**执行完成后，就立即开始响应硬件中断信息**，并调用注册的中断处理函数。为了避免可能的**逻辑问题**或**未初始化数据被使用**，通常建议**先调用 `tasklet_init()`** 初始化好对应的 `tasklet` 资源，**再调用 `request_irq()`** 注册硬件中断。
3. `tasklet_init` 注册的函数中，**不可以调用可能引起休眠的函数**。但如果是 `mdelay()` 这种**单纯消耗 `CPU` 进行阻塞的延时函数，那么是可行的**，不过不建议。（需要注意，**当软中断执行时间超过 `2ms`，剩余任务将会被转移至 `ksoftirqd` 内核线程中执行**）

| 类型   | 描述                         |
| ------ | ---------------------------- |
| 作用   | 对 `tasklet` 动态初始化      |
| `t`    | 指向 `tasklet_struct` 的指针 |
| `func` | `tasklet` 的处理函数         |
| `data` | 传递给处理函数的参数         |
| 返回值 | 无                           |

```c
void tasklet_init(struct tasklet_struct *t, void (*func)(unsigned long), unsigned long data);

// 示例
struct tasklet_struct mytasklet;
void mytasklet_func(unsigned long data)
{
    printk("data is %ld\n", data);
    // msleep(3000);
    mdelay(1000);
}

irqreturn_t test_interrupt(int irq, void *args)
{
    // ...
    tasklet_schedule(&mytasklet); // 调度tasklet执行
    // ...
}

static int __init interrupt_irq_init(void)
{
	// ...
    tasklet_init(&mytasklet, mytasklet_func, 1);
    ret = request_irq(irq, test_interrupt, IRQF_TRIGGER_RISING, "test", NULL);
	//...
}
```

4. 当我们不再需要使用一个 `tasklet` 时，可以调用如下函数进行注销。
5. 需要注意 : **该函数是阻塞的，会等待 `tasklet` 的当前执行完成**。因此**不建议**在中断上下文调用，因为它可能**导致内核等待**，进而引发不可预期的问题。
6. `tasklet_kill` 函数的作用是等待指定的 `tasklet` 完成执行，并确保它不会再次被调度。如果`tasklet` 处于禁用状态（即被 `tasklet_disable` 禁用），它将无法被调度执行。在这种情况下，直接调用 `tasklet_kill` 可能会导致函数无法等待 `tasklet` 完成（因为它从未开始执行），从而引发逻辑错误。因此，在调用 `tasklet_kill` 之前，先调用 `tasklet_enable` 确保 `tasklet` 处于可调度状态。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 注销一个已经初始化好的 `tasklet`<br />- 如果 `tasklet` 已被调度但尚未运行，`tasklet_kill` 会将其从调度队列中移除<br />- 如果 `tasklet` 当前正在运行，`tasklet_kill` 会等待其执行完成，然后再移除它，确保不会中途打断其执行 |
| `t`    | 指向 `tasklet_struct` 的指针                                 |
| 返回值 | 无                                                           |

```c
void tasklet_kill(struct tasklet_struct *t);
// 示例
static void __exit interrupt_irq_exit(void)
{
  // ...
  free_irq(irq, NULL);
  tasklet_kill(&mytasklet);
  // ...
}
```

## 调度函数

1. 该函数会将指定的 `tasklet` **标记为待执行(`pending`)**，并将其添加到内核的软中断队列中。
2. 调用 `tasklet_schedule()` 函数只是标记为**待执行(`pending`)**，因此**不会立刻执行**对应的 `tasklet` 的处理函数。**实际的执行时间取决与内核的调度和处理机制**。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 将指定的 `tasklet` 标记为待执行(`pending`)，并将其添加到内核的软中断队列中 |
| `t`    | 指向 `tasklet_struct` 的指针                                 |
| 返回值 | 无                                                           |

```c
void tasklet_schedule(struct tasklet_struct *tasklet);
```

## 暂时禁用/启用

1. **暂时禁用**指定的 `tasklet`，这意味着即使我们调用 `tasklet_schedule()` 函数来调度该 `tasklet` 已经被调度，也不会被执行。

| 类型   | 描述                         |
| ------ | ---------------------------- |
| 作用   | 暂时禁用指定的 `tasklet`     |
| `t`    | 指向 `tasklet_struct` 的指针 |
| 返回值 | 无                           |

```c
void tasklet_disable(struct tasklet_struct *tasklet);
```

2. 当一个 `tasklet` 被暂停使用，现在我们希望重新启动该 `tasklet` 运行，那么可以调用如下函数。

| 类型   | 描述                         |
| ------ | ---------------------------- |
| 作用   | 启用被暂时禁用的 `tasklet`   |
| `t`    | 指向 `tasklet_struct` 的指针 |
| 返回值 | 无                           |

```c
void tasklet_enable(struct tasklet_struct *t);
```

# 综合示例

```c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
// #include <linux/delay.h>

int irq;
struct tasklet_struct mytasklet;

void mytasklet_func(unsigned long data)
{
  printk("data is %ld\n", data);
  // msleep(3000);  // tasklet 中不可以调用可能引起休眠的函数
}


irqreturn_t test_interrupt(int irq, void *args)
{
  printk("This is test_interrupt\n");
  tasklet_schedule(&mytasklet);
  return IRQ_RETVAL(IRQ_HANDLED);
}

static int interrupt_irq_init(void)
{
  int ret;
  irq = gpio_to_irq(112);

  tasklet_init(&mytasklet, mytasklet_func, 1);
  ret = request_irq(irq, test_interrupt, IRQF_TRIGGER_RISING, "test", NULL);
  if (ret < 0) {
    printk("request_irq is error\n");
    return -1;
  }
  return 0;
}

static void interrupt_irq_exit(void)
{

  free_irq(irq, NULL);
  // 如果一个 tasklet 被禁用，直接调用 tasklet_kill 可能会导致函数无法正确等待 tasklet 结束，进而引发死锁或资源泄露
  // 因为这里 tasklet 并没有被禁止，因此最终退出时，该函数调用是可选的
  tasklet_enable(&mytasklet); // 使能tasklet（可选）
  tasklet_kill(&mytasklet);
  printk("bye bye\n");
}

module_init(interrupt_irq_init);
module_exit(interrupt_irq_exit);

MODULE_LICENSE("GPL");
```



