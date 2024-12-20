# 基础概念

1. 软中断是实现中断下半部的方法之一，但是因为**软中断资源有限**，对应的**中断号并不多**，所以一般都是用在了**网络驱动设备**和**块设备**中。
2. 软中断（`Softirqs`）是**静态定义且不能动态修改的**的，**软中断通常是针对系统的核心任务和重要事件（如网络包处理、定时器等）设计的，这些任务在系统的生命周期中通常是固定的**。因此，内核并**没有提供专门的函数来注销软中断处理程序**。
3. **软中断（`Softirqs`）更倾向与性能，可以在多个 `CPU` 并行执行，因此要考虑重入问题，适当时候需要加锁进行保护**。
4. 软中断（`Softirqs`）的**中断号优先级越小，优先级越高**。我们**可以添加自定义软中断**，只不过需要**重新编译 `Linux` 内核**。但`Linux` 内核开发人员不希望驱动开发者新增软件中断类型，因此驱动开发者**多数时候都是使用的 `tasklet` ，工作队列，中断线程化作为中断下半部**。
5. **软中断的执行时机是上半部返回的时候**。
6. 软中断和基于软中断的 `tasklet` 如果 **执行超过超过 `2ms`** /**更高优先级的任务就绪**/**待处理软中断还有10个**， `Linux` 内核会把后续软中断**放入 `ksoftirqd` 内核线程中执行**。这一设计是为了防止单个软中断占用过多的 CPU 时间，从而影响系统整体性能。

# API 介绍

## 注册软中断

1. 该函数为某一类型的软中断注册应用函数，设置软中断层的监听和处理策略。
2. 再次强调，**软中断都是静态定义，因此并没有动态的删除函数**。

| 类型     | 描述                                                         |
| -------- | ------------------------------------------------------------ |
| 作用     |                                                              |
| `nr`     | 软中断类型的标识，将软中断分为多个类型；指定将使用的软中断类型<br />- `HI_SOFTIRQ` : 高优先级中断<br />- `TIMER_SOFTIRQ` : 定时器软中断<br />- `NET_TX_SOFTIRQ` : 网络传输**发送**软中断<br />- `NET_RX_SOFTIRQ` : 网络传输**接收**软中断<br />- `BLOCK_SOFTIRQ` : 块设备软中断<br />- `IRQ_POLL_SOFTIRQ` : 支持 `IO` 轮询的块设备软中断<br />- `TASKLET_SOFTIRQ` : 低优先级的小任务<br />- `SCHED_SOFTIRQ` : 调度软中断，**用于处理器之间的均衡负载**<br />- `HRTIMER_SOFTIRQ` : 高精度定时器，但这种软中断**已经被废弃**，目前**中断处理程序的上半部处理高精度定时器**；保留的原因是当作**工具来保存编号**<br />- `RCU_SOFTIRQ` : `RCU` 软中断，**该中断最好是软中断的最后一个**<br />- `NR_SOFTIRQS` : 表示软中断的总数 |
| `action` | 处理软中断的函数；该函数接受一个 `struct softirq_action` 类型的参数 |
| 返回值   | 无                                                           |

```c
void open_softirq(int nr, void (*action)(struct softirq_action *));
```

## 触发软中断

1. 触发软中断，一般在中断上半段调用，不过可以其他地方被调用。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 触发软中断                                                   |
| `nr`   | 要触发的软中断中断编号，与 `open_softirq()` 中注册的软中断类型一致 |
| 返回值 | 无                                                           |

```c
void raise_softirq(unsigned int nr);
```

2. **该函数必须在禁用硬中断的情况下调用**。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 触发软中断（中断禁用情况下）                                 |
| `nr`   | 要触发的软中断中断编号，与 `open_softirq()` 中注册的软中断类型一致 |
| 返回值 | 无                                                           |

```c
void raise_softirq_irqoff(unsigned int nr);
```

3. 虽然**大部分的使用场景都是在中断处理函数中**（也就是说关闭本地CPU中断）来执行 `softirq` 的触发动作，但是，这不是全部，在**其他的上下文中也可以调用 `raise_softirq`**。因此，触发 `softirq` 的接口函数有两个版本，一个是 **`raise_softirq` ，有关中断的保护**，另外一个是 **`raise_softirq_irqoff` ，调用者已经关闭了中断**。

```c
// raise_softirq 本质就是调用 raise_softirq_irqoff
void raise_softirq(unsigned int nr)
{
	unsigned long flags;

	local_irq_save(flags);
	raise_softirq_irqoff(nr);
	local_irq_restore(flags);
}
```

