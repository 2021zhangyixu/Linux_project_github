# 基础概念

## 工作队列与 Tasklet 区别

1. 工作队列（`Workqueues`） 与 任务（`Tasklet`）最大的区别是，是否可以进行休眠。任务（`Tasklet`）工作在**软中断上下文**，而工作队列（`Workqueues`）工作在**内核线程**。因此，工作队列可以执行耗时的任务。
2. 工作队列的**实时性没有 `tasklet` 高**。

## 工作原理

1. **工作队列**可以理解为工厂的**传送带**，而**工作项**则是传送带上需要被处理的**货物**。
2. `Linux` 启动过程中会创建一个**工作者内核线程**，这个**线程创建以后处于 `sleep`  状态**下。当有**工作项**在**工作队列**上需要进行处理，会唤醒这个线程处理工作工作项。

## 共享工作队列与自定义工作队列

3. 内核中，工作队列分为**共享工作队列**和**自定义工作队列**。
   - **共享工作队列** : 共享工作队列由 **`Linux` 内核管理**的全局工作队列，用于处理内核中一些系统级任务。工作队列是内核一个默认工作队列，可以由**多个内核组件和驱动程序共享使用**。
   - **自定工作队列** : 由**内核创建**特定工作队列，用于处理特定的任务。自定工作队列通常与特定的内核模块或驱动程序相关联，用于执行该模块或驱动程序相关的任务。
2. 自定义工作队列比共享工作队列**具有更大的灵活性**，但它**资源消耗大**，如果**不是**需要**隔离**和**特殊配置**的工作，**建议采用共享内存**。

| 类别 | 共享工作队列                                                 | 自定工作队列                                 |
| ---- | ------------------------------------------------------------ | -------------------------------------------- |
| 优点 | `Linux` 自身就有，使用起来非常简单                           | 不会被其他工作影响                           |
| 缺点 | 该队列是共享的，因此会受到其他工作影响，导致任务长时间无法执行 | 工作队列需要自行创建，对系统会引起额外的开销 |

## 延迟工作

1. 工作队列分为共享工作队列和自定义工作队列。**当我们把工作放入工作队列后，工作是稍后执行**。但我们存在一些应用场景，某个任务需要花费一定的时间，**任务并不需要马上执行，那么我们就可以使用延迟工作**。
2. **延迟工作不仅可以在自定义工作队列中实现，还可以在共享工作队列中实现**。
4. 延迟工作的应用场景 : 
   - 延迟工作常用于**处理那些需要花费较长时间的任务**，比如发送电子邮件，处理图像等。通过将这些任务放入队列中并延迟执行，可以**避免阻塞应用程序的主线程**，**提高系统的响应速度**。
   - 延迟工作可以**用来执行定时任务**，比如按键消抖后读取（不过需注意 `Linux` 中按键驱动是采用的软件定时器消抖）。通过将任务设置为在未来的某个时间点执行，**提高系统的可靠性和效率**。

# 共享工作队列

## API 介绍

### 工作项初始化

1. 在实际的驱动开发中，我们**只需要定义工作项**即可，关于工作队列和工作线程我们不需要去管。
2. 创建共享工作队列很简单，直接创建一个 `work_struct` 结构体变量，然后调用如下宏进行**动态初始化**即可。

| 类型    | 描述                             |
| ------- | -------------------------------- |
| 作用    | 初始化一个工作队列中的工作       |
| `_work` | 指向 `struct work_struct` 的指针 |
| `_func` | 工作项处理函数                   |
| 返回值  | 无                               |

```c
INIT_WORK(_work, _func);
```

2. 我们也可以调用如下宏一次性完成共享工作队列的**静态创建和初始化**。

| 类型    | 描述                       |
| ------- | -------------------------- |
| 作用    | 初始化一个工作队列中的工作 |
| `_work` | 工作项变量名字             |
| `_func` | 工作项处理函数             |
| 返回值  | 无                         |

```c
DECLARE_WORK(name, func);

// 驱动示例

DECLARE_WORK(test_work, test_work_fun);

void test_work_fun(struct work_struct *work)
{
  msleep(1000);
  printk("This is test_work\n");
}

irqreturn_t test_interrupt(int irq, void *args)
{
  // ...
  schedule_work(&test_work);
  return IRQ_RETVAL(IRQ_HANDLED);
}

static int interrupt_irq_init(void)
{
    // ...
    request_irq(irq, test_interrupt, IRQF_TRIGGER_RISING, "test", NULL);
}
```

### 工作项加入调度

1. 将一个工作项调度到共享工作队列中。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 将一个工作存放入系统工作队列中                               |
| `work` | 指向 `work_struct` 结构体的指针，工作项                      |
| 返回值 | 工作成功加入共享工作队列，返回 `true` ; 该工作已在共享工作队列，返回 `false` |

```c
bool schedule_work(struct work_struct *work);
```

### 取消调度未执行的工作项

1. 该函数用于取消一个已经调度但尚未执行完成的工作。
2. 它可以确保目标工作要么已经完成执行，要么被成功取消，**一般用于驱动程序的释放函数中**，从而**避免因工作正在执行导致的不确定行为**。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 取消一个已经调度但尚未执行完成的工作                         |
| `work` | 向需要取消的 `struct work_struct` 的指针                     |
| 返回值 | 该工作已成功取消（工作未开始执行），返回 `true` ; 工作已经在执行，无法取消，返回 `false` |

```c
bool cancel_work_sync(struct work_struct *work);
```

## 综合示例

```c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

int irq;

struct work_struct test_work;

void test_work_fun(struct work_struct *work)
{
  msleep(1000);
  printk("This is test_work\n");
}

irqreturn_t test_interrupt(int irq, void *args)
{
  printk("This is test_interrupt\n");
  schedule_work(&test_work);
  return IRQ_RETVAL(IRQ_HANDLED);
}

static int interrupt_irq_init(void)
{
  int ret;
  irq = gpio_to_irq(112);
  INIT_WORK(&test_work, test_work_fun);
  ret = request_irq(irq, test_interrupt, IRQF_TRIGGER_RISING, "test", NULL);
  if (ret < 0) {
    printk("request_irq is error\n");
    return -1;
  }
  return 0;
}

static void interrupt_irq_exit(void)
{
    if (work_pending(&test_work)) {
        printk("Work is still pending, canceling...\n");
        cancel_work_sync(&my_work);
    } else {
        while(work_busy(&test_work)) { 
            printk("Work is currently being executed.\n");
        }
    }
    free_irq(irq, NULL);
    printk("bye bye\n");
}

module_init(interrupt_irq_init);
module_exit(interrupt_irq_exit);
```

# 自定义工作队列

## API 介绍

### 创建自定义工作队列

1. 如果我们希望**并发处理**某个任务，就可以调用如下 `API` 来创建自定义工作队列。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 创建一个在每个 `CPU` 上都有一个工作线程的工作队列，允许并行处理工作项 |
| `name` | 工作队列的名称                                               |
| 返回值 | 成功，返回一个指向新创建的 `struct workqueue_struct` 的指针；失败，返回 `NULL` |

```c
struct workqueue_struct *create_workqueue(const char *name);
```

2. 如果我们希望创建一个**单线程**的、**有序**的工作队列，所有工作项按顺序执行，那么就调用如下 `API` 。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 只给一个 `CPU` 创建一个 `CPU` 相关的工作队列                 |
| `name` | 工作队列的名称                                               |
| 返回值 | 成功，返回一个指向新创建的 `struct workqueue_struct` 的指针；失败，返回 `NULL` |

```c
create_singlethread_workqueue(const char *name);
```

### 销毁自定义工作队列

1. 当我们创建了自定义工作队列，并且不再想使用后，可以调用如下函数进行销毁。

| 类型   | 描述                                                  |
| ------ | ----------------------------------------------------- |
| 作用   | 销毁自定义工作队列                                    |
| `wq`   | 需要销毁的工作队列指针（`struct workqueue_struct *`） |
| 返回值 | 无                                                    |

```c
void destroy_workqueue(struct workqueue_struct *wq);
```

### 工作项加入调度

1. 当我们已经拥有了自定义工作队列以后，可以调用如下 `API` 将要进行延迟处理的工作项放到自定义工作队列中。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 将工作项放到指定的工作队列中                                 |
| `cpu`  | 指定任务应该在哪个 CPU 上执行。如果使用 `WORK_CPU_UNBOUND`（通常为 `-1`），任务会在任意可用的 CPU 上运行 |
| `wq`   | 需要销毁的工作队列指针（`struct workqueue_struct *`）        |
| `work` | 指向 `work_struct` 的指针，表示要提交的任务                  |
| 返回值 | 任务成功加入自定义工作队列，返回 `true` ; 否则返回 `false`   |

```c
bool queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *work);

// 我们也可以使用如下函数，本质就是对当前的 API 进一步封装
static inline bool queue_work(struct workqueue_struct *wq,
			      struct work_struct *work)
{
	return queue_work_on(WORK_CPU_UNBOUND, wq, work);
}
```

### 取消调度但未被执行的工作项

1. 要取消一个已经调度的工作，如果被取消的工作已经正在执行，则会等待他执行完成再返回。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 要取消一个已经调度的工作                                     |
| `wq`   | 需要被取消的工作队列指针（`struct workqueue_struct *`）      |
| 返回值 | 任务成功被取消，返回 `true` ; 任务未被取消（可能已经执行或从未被排入队列），返回 `false` |

```c
bool cancel_work_sync(struct work_struct *work);
```

### 刷新自定义工作队列中工作项

1. 当我们希望加入工作队列的任务**快点执行**，我们可以调用如下函数。一般该函数**多用于销毁自定义队列之前，确保自定义队列被安全的销毁**。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 等待一个工作队列中的所有已提交的工作项完成执行，**该函数将会引起阻塞** |
| `wq`   | 需要被执行的工作队列指针（`struct workqueue_struct *`）      |
| 返回值 | 无                                                           |

```c
void flush_workqueue(struct workqueue_struct *wq);
```

## 综合示例

```c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

int irq;
struct workqueue_struct *test_workqueue;
struct work_struct test_workqueue_work;

void test_work(struct work_struct *work)
{
  msleep(1000);
  printk("This is test_work\n");
}

irqreturn_t test_interrupt(int irq, void *args)
{
  printk("This is test_interrupt\n");
  queue_work(test_workqueue, &test_workqueue_work);
  return IRQ_RETVAL(IRQ_HANDLED);
}

static int interrupt_irq_init(void)
{
  int ret;
  irq = gpio_to_irq(112);

  test_workqueue = create_workqueue("test_workqueue");
  INIT_WORK(&test_workqueue_work, test_work);

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
  cancel_work_sync(&test_workqueue_work);
  flush_workqueue(test_workqueue);
  destroy_workqueue(test_workqueue);
  printk("bye bye\n");
}

module_init(interrupt_irq_init);
module_exit(interrupt_irq_exit);

MODULE_LICENSE("GPL");
```

# 两种工作队列使用上的异同

## 工作队列创建与销毁

- **共享工作队列** : 由**内核创建**特定工作队列，因此驱动开发人员是**不需要对其进行创建和销毁**。不过函数退出的时候，记得**将工作项从共享工作队列销毁**。
- **自定义工作队列** : 由驱动开发人员创建或者销毁。使用如下 `API` 进行创建

```c
/*** 共享工作队列 ***/
// 驱动程序退出时，因其工作项相关信息也将会被清除。为了防止共享工作队列处理已经被销毁的工作项，应当做如下保护

if (work_pending(&my_work)) { // 检查工作项处于调度状态，如果是，那么取消调度
    printk("Work is still pending, canceling...\n");
    cancel_work_sync(&my_work);
} else {
    // 如果不处于调度状态，检查是否是工作状态，如果是，等待其运行结束，否则直接处理后续清理任务
    while(work_busy(&my_work)) { 
    	printk("Work is currently being executed.\n");
	}
}

/*** 自定义工作队列 ***/
// 创建
struct workqueue_struct *create_workqueue(const char *name);
// 销毁
/* 1. 先将所有的工作项进行取消
 * 2. 如果取消失败的工作项，说明要么没有进入调度，要么正在执行，那么让正在执行的工作项完成
 * 3. 等上述步骤完成后，方可安全的销毁自定义工作队列
 */
bool cancel_work_sync(struct work_struct *work);
void flush_workqueue(struct workqueue_struct *wq);
void destroy_workqueue(struct workqueue_struct *wq);
```

## 工作项创建与初始化

1. 无论是共享工作队列，亦或是自定义工作队列，**两者对工作项的创建和初始化都是一样的**。

```c
struct work_struct test_work;
static int interrupt_irq_init(void)
{
  // ...
  INIT_WORK(&test_work, test_work_fun);
  return 0;
}
```

## 工作项调度

1. **共享工作队列的工作项加入调度调用的 `schedule_work` 其底层其实就是调用的自定义工作队列的 `queue_work`**。

```c
// 共享工作队列
static inline bool schedule_work(struct work_struct *work)
{
	return queue_work(system_wq, work);
}
```

# 延迟工作

## API 介绍

### 工作项创建

1. **静态定义**并初始化延迟工作项。

| 类型            | 描述                           |
| --------------- | ------------------------------ |
| 作用            | 静态定义并初始化一个延迟工作项 |
| `work_name`     | 延迟工作项变量名               |
| `work_function` | 需要延迟工作的处理函数         |
| 返回值          | 无                             |

```c
DECLARE_DELAYED_WORK(work_name, work_function);
```

2. 当我们希望**动态定义**并初始化延迟工作项，可以使用如下宏。
3. 需要注意，当我们调用 `INIT_DELAYED_WORK` 这是对 `delayed_work.work` 成员进行初始化，**并没有分配额外的内存**，因此没有对应的内存释放函数。

| 类型    | 描述                                                         |
| ------- | ------------------------------------------------------------ |
| 作用    | 动态分配在运行时初始化的延迟工作                             |
| `dwork` | 指向一个 `struct delayed_work` 的指针或实例，表示要初始化的延迟工作项 |
| `func`  | 需要延迟工作的处理函数                                       |
| 返回值  | 无                                                           |

```c
INIT_DELAYED_WORK(dwork, func);
```

### 延迟工作项加入调度

1. 当我们是在一个**共享队列**中调度延迟工作，应当使用如下函数。

| 类型    | 描述                                                         |
| ------- | ------------------------------------------------------------ |
| 作用    | 动态分配在运行时初始化的延迟工作                             |
| `dwork` | 指向一个 `struct delayed_work` 的指针或实例，表示要调度的延迟工作项 |
| `delay` | 延迟时间（以 `jiffies` 为单位），工作项将在此时间后执行；可以通过 `msecs_to_jiffies()` 将毫秒转换为 `jiffies` |
| 返回值  | 成功返回 `true` ，否则返回 `false`                           |

```c
bool schedule_delayed_work(struct delayed_work *dwork,unsigned long delay);
```

2. 如果我们实在要给**自定义队列**中调度延迟工作，应当使用如下函数。

| 类型    | 描述                                                         |
| ------- | ------------------------------------------------------------ |
| 作用    | 动态分配在运行时初始化的延迟工作                             |
| `wq`    | 指向 `workqueue_struct` 的指针，表示要将工作项添加到的工作队列 |
| `dwork` | 指向一个 `struct delayed_work` 的指针或实例，表示要调度的延迟工作项 |
| `delay` | 延迟时间（以 `jiffies` 为单位），工作项将在此时间后执行；可以通过 `msecs_to_jiffies()` 将毫秒转换为 `jiffies` |
| 返回值  | 成功返回 `true` ，否则返回 `false`                           |

```c
bool queue_delayed_work(struct workqueue_struct *wq, struct delayed_work *dwork, unsigned long delay);
```

### 取消延迟工作项调度

1. 如果需要取消调度，就应该使用如下函数。**无论是共享工作队列还是自定义工作队列，这个 `API` 都能正确地取消该工作项**。

| 类型    | 描述                                                         |
| ------- | ------------------------------------------------------------ |
| 作用    | 动态分配在运行时初始化的延迟工作                             |
| `dwork` | 指向一个 `struct delayed_work` 的指针或实例，表示要取消调度的延迟工作项 |
| 返回值  | 成功取消返回 `true` ，否则返回 `false`                       |

```c
bool cancel_delayed_work_sync(struct delayed_work *dwork);
```

## 综合示例

```c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

int irq;
struct workqueue_struct *test_workqueue;
struct delayed_work test_workqueue_work;

void test_work(struct work_struct *work)
{
  msleep(1000);
  printk("This is test_work\n");
}

irqreturn_t test_interrupt(int irq, void *args)
{
  printk("This is test_interrupt\n");
  queue_delayed_work(test_workqueue, &test_workqueue_work, msecs_to_jiffies(3)); 
  return IRQ_RETVAL(IRQ_HANDLED);
}

static int interrupt_irq_init(void)
{
  int ret;
  irq = gpio_to_irq(112);
  printk("irq is %d\n", irq);

  test_workqueue = create_workqueue("test_workqueue"); 
  INIT_DELAYED_WORK(&test_workqueue_work, test_work);  
  ret = request_irq(irq, test_interrupt, IRQF_TRIGGER_RISING, "test", NULL);
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
  cancel_delayed_work_sync(&test_workqueue_work);
  flush_workqueue(test_workqueue);
  destroy_workqueue(test_workqueue);
  printk("bye bye\n");
}

module_init(interrupt_irq_init);
module_exit(interrupt_irq_exit);

MODULE_LICENSE("GPL");
```

# 并发管理

1. 通过 `alloc_workqueue` 并发管理工作队列，我们可以同时处理多个任务或工作，提高系统的并发性和性能。每个任务独立地从队列中获取并执行，这种解耦使得整个系统更加高效、灵活，并且能够更好地应对多任务的需求。
2. 我们可以利用如下函数**代替创建自定义工作队列**的 `API` ，该 `API` 将会创建一个新的**自定义工作队列**（`struct workqueue_struct`），并根据指定的参数配置工作队列的行为，例如**并发性**、**是否绑定到 `CPU`** 等。
3. 通过如下 `API` 我们可以**实现对工作项排序**。

| 类型         | 描述                                                         |
| ------------ | ------------------------------------------------------------ |
| 作用         | 创建一个自定义工作队列                                       |
| `fmt`        | 工作队列的名称格式字符串                                     |
| `flags`      | 指定工作队列的属性和行为，常见的可选值包括：<br />- `WQ_UNBOUND` : 创建一个无绑定的工作队列，可以在任何 `COU` 上运行；适合需要并发或跨 `CPU` 执行的任务，此类西任务执行时间较长，需要充分利用系统多核资源；虽然这种方法可以**增加并发性**，但同时也**增加了调度的复杂性**<br />- `WQ_FREEZABLE` : 与**电源管理相关，表示当前工作队列可被冻结**<br />- `WQ_MEM_RECLAIM` : 表示**当前队列可能用于内存回收**，这意味着当系统内存不足时，当前工作队列可能会被**优先处理，用以帮助释放内存资源**；一般用于**内存回收**、**文件系统**写回场景<br />- `WQ_HIGHPRI` : 表示**高优先级工作队列**<br />- `WQ_CPU_INTENSIVE` : 表示该工作队列用于 `CPU` 密集型任务，一般用于**特别耗时**的工作 |
| `max_active` | 控制工作队列的并发性，即允许同时运行的任务数量；如果设置为 **0 ，表示使用默认值**；如果设置**为1**，则表示队列每次只会执行一个工作项，保证工作项按**顺序逐个处理** |
| 返回值       | 返回一个指向 `workqueue_struct` 的指针，表示新创建的工作队列；若创建失败，则返回 `NULL` |

```c
struct workqueue_struct *alloc_workqueue(const char *fmt, unsigned int flags, int max_active);
```

# 工作队列传参

1. 工作队列传参，本质上就是**利用的 `container_of` 宏，通过结构体成员的指针，反推出包含该成员的整个结构体的起始地址**。
2. 在如下队列中，我们申明一个结构体，然后正常的初始化该队列。之后当我们进入到中断下半段的时候，就可以利用  `container_of` 宏进行如下操作，得到我们想要获取的数值。最终实现工作队列的传参。

```c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

int irq;

struct work_data
{
  struct work_struct test_work;
  int a;
  int b;
};

struct work_data test_workqueue_work;

struct workqueue_struct *test_workqueue;

void test_work(struct work_struct *work)
{
  struct work_data *pdata;
  pdata = container_of(work, struct work_data, test_work);

  printk("a is %d", pdata->a);
  printk("b is %d", pdata->b);
}

irqreturn_t test_interrupt(int irq, void *args)
{
  printk("This is test_interrupt\n");
  queue_work(test_workqueue, &test_workqueue_work.test_work);
  return IRQ_RETVAL(IRQ_HANDLED);
}

static int interrupt_irq_init(void)
{
  int ret;
  irq = gpio_to_irq(112); // 将GPIO映射为中断号
  printk("irq is %d\n", irq);

  test_workqueue = create_workqueue("test_workqueue");
  INIT_WORK(&test_workqueue_work.test_work, test_work);
  ret = request_irq(irq, test_interrupt, IRQF_TRIGGER_RISING, "test", NULL);
  if (ret < 0) {
    printk("request_irq is error\n");
    return -1;
  }

  test_workqueue_work.a = 1;
  test_workqueue_work.b = 2;

  return 0;
}

static void interrupt_irq_exit(void)
{
  free_irq(irq, NULL);
  cancel_work_sync(&test_workqueue_work.test_work);
  flush_workqueue(test_workqueue);
  destroy_workqueue(test_workqueue); 
  printk("bye bye\n");
}

module_init(interrupt_irq_init);
module_exit(interrupt_irq_exit);

MODULE_LICENSE("GPL");
```

