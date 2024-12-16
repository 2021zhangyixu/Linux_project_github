# 基础概念

## 中断作用

1. 中断是指 `CPU` 正常运行期间，由**外部（硬件中断）**和**内部（软中断）**事件引起的一种机制。
2. 中断机制赋予了我们处理意外情况的能力，如果我们能充分利用这个机制，将能够同时完成多个任务，从而**提高并发处理能力和资源利用率**。

## 中断的层级划分

1. 一个完整的中断子系统包括四个层次 : 

   - **用户层** : 该层是中断的使用者，主要**包含各类设备驱动，这些驱动程序通过中断相关 `API` 进行中断的申请和注册**。当外设触发中断时，用户层驱动程序也会进行相应的回调处理，执行特定的操作。**这一层是驱动工程师日常使用的**。
   - **通用层（框架层）** : 与硬件无关的层次。该层提供统一个接口和功能，用于管理和处理中断，使得驱动程序能够在不同的硬件平台上复用。**这一层主要是 Linux 内核开发者负责**。
   - **硬件相关层** : 该层包含两部分代码，处理器架构相关代码和中断控制器驱动代码。**这一层是原厂工程师负责**。
   - **硬件层** : 硬件层位于最底层，与具体的硬件连接相关。包含外设与 `SOC` 的物理连接。硬件层的设计和实现决定了中断信号的传递方式和硬件的处理能力。

   ![interrupt_arch](./img/irq_arch.png)

## 抢占机制

1. `Linux` **不支持中断嵌套**。因为如果出现了突发事件，**多个中断短时间同时触发，会导致多层中断嵌套，栈会极速增大，最终栈溢出**。
2. 虽然 `Linux` 不支持中断嵌套，但是一些 **`GIC` 控制器会支持抢占**。如果低优先级任务已经进入了 `CPU` ，高优先级任务依旧只能等着。但是如果低优先级任务还没进入 `CPU` ，此时 `GIC` 又来了一个高优先级任务，最终 `GIC` 会将高优先级任务传递给 `CPU`。
3. 因此，我们可以得出结论 : 

   - 高优先级中断无法抢占**正在执行**的低优先级中断。
   - 都是处于 **`pending` 状态**的中断，优先级高的中断先执行。
   - **同优先级，同 `pending`** 状态，选择**硬件中断号 IO 最小**的一个发给 `CPU` 执行。

## 中断上下文

1. 中断执行需要快速响应，但并不是所有的中断都可以迅速完成。而 **`Linux` 中不支持中断嵌套，意味着正式处理中断前就会屏蔽其他的中断**，直到中断处理完成后才重新允许中断。
2. 如果**中断执行时间过长就会导致部分中断无法被响应，可能导致死机**。为了解决这个问题，中断分为上下段。上段执行时间短，且需要尽快执行的事情。下段为一些相对耗时较长的事情。
3. **上半部是不能被中断的，下半部可被中断**。
4. 上半部是由**硬件请求**，也就是我们常说的**硬中断**，特点是快速执行。下半部是由**内核触发**，也就是我们常说的**软中断**，特点是延迟执行。
7. 下半段可通过**软中断** ，**`tasklet`**，**工作队列**来实现的。**`tasklet` 是基于软中断实现的**。

## 下半部实现

| 特性       | 软中断（`Softirqs`）                                         | 任务（`Tasklet`）                                            | 工作队列（`Workqueues`）                 | 中断线程化（`Interrupt Threading`）      |
| ---------- | ------------------------------------------------------------ | ------------------------------------------------------------ | ---------------------------------------- | ---------------------------------------- |
| 执行上下文 | **中断上下文**中执行；**不能**调用可能**引起睡眠**的函数，**不建议**调用**引起阻塞的函数** | **中断上下文**中执行；**不能**调用可能**引起睡眠**的函数，**不建议**调用**引起阻塞的函数** | **内核线程**；**可以进行阻塞等待和睡眠** | **内核线程**；**可以进行阻塞等待和睡眠** |
| 并行性     | **向多个 `CPU` 注册，要考虑并发问题**                        | 向一个 `CPU` 注册                                            | 默认同一个 `CPU` 或指定 `CPU`            |                                          |
| 定义       | 软中断是**编译时静态定义**，运行时**不可以**进行添加或删除   | **`tasklet`** 支持静态定义，也可以通过 `tasklet_init` 动态初始化 | \                                        |                                          |
| 实时性     | 优秀                                                         | 良好                                                         | 差                                       | 较差                                     |
| 开销       | 最小                                                         | 比软中断多一点                                               | 最大                                     | 比工作队列小一点                         |
| 适用场景   | 软中断的**资源有限**，对应的中断号不多，一般用在**网络设备驱动**、**块设备驱动**当中 | 驱动开发，简单任务                                           | **需要延迟工作**，且复杂、耗时任务       | **不需要延迟工作**，且复杂、耗时任务     |

### 任务（`Tasklet`）

1. `Tasklets` 是一种基于**软中断（`Softirqs`）**的特殊机制，被广泛应用于中断下半部。
2. 它**在多核处理器上可以避免并发问题，因为 `Tasklet` 会绑定一个 `CPU` 执行**。
3. `Tasklet` 绑定的中断函数**不可以调用可能导致休眠的函数**，例如 `msleep()` ，否则会引起内核异常。
4. 虽然**不可以调用引起休眠的函数**，但**可以调用** `mdelay()` 通过消耗 `CPU` 实现的**纯阻塞延时函数**。不过依旧不建议在中断中调用延时函数，因为**中断要求快进快出**。
5. `tasklet` 可以静态初始化，也可以动态初始化。如果**使用静态初始化，那么就无法动态销毁**。因此在不需要使用 `tasklet` 时，应当避免使用此方法。

###   软中断（`Softirqs`）

1. 软中断是实现中断下半部的方法之一，但是因为**软中断资源有限**，对应的**中断号并不多**，所以一般都是用在了**网络驱动设备**和**块设备**中。
2. 软中断（`Softirqs`）是**静态定义且不动态修改的**的，**软中断通常是针对系统的核心任务和重要事件（如网络包处理、定时器等）设计的，这些任务在系统的生命周期中通常是固定的**。因此，内核并**没有提供专门的函数来注销软中断处理程序**。
3. **软中断（`Softirqs`）更倾向与性能，可以在多个 `CPU` 并行执行，因此要考虑重入问题，适当时候需要加锁进行保护**。
4. 软中断（`Softirqs`）的**中断号优先级越小，优先级越高**。我们**可以添加自定义软中断**，只不过需要重新编译 `Linux` 内核。但`Linux` 内核开发人员不希望驱动开发者新增软件中断类型，因此驱动开发者**多数时候都是使用的 `tasklet` ，工作队列，中断线程化作为中断下半部**。
5. **软中断的执行时机是上半部返回的时候**。
6. 软中断的**执行时间不可以超过 2 ms，并且最多执行 10 次**。**如果在这10次内仍然有待处理的软中断，系统将唤醒一个名为 `ksoftirqd` 的内核线程来处理剩余的软中断**。这一设计是为了防止单个软中断占用过多的 CPU 时间，从而影响系统整体性能。

### 工作队列（`Workqueues`）

#### 共享工作队列和自定义工作队列

1. 工作队列（`Workqueues`） 与 任务（`Tasklet`）最大的区别是，是否可以进行休眠。任务（`Tasklet`）工作在**软中断上下文**，而工作队列（`Workqueues`）工作在**内核线程**。因此，工作队列可以执行耗时的任务。
2. `Linux` 启动过程中会创建一个**工作者内核线程**，这个线程创建以后处于 `sleep`  状态下。当有工作需要进行处理，会唤醒这个线程处理工作。
3. 内核中，工作队列分为**共享工作队列**和**自定义工作队列**。
   - **共享工作队列** : 共享工作队列由 **`Linux` 内核管理**的全局工作队列，用于处理内核中一些系统级任务。工作队列是内核一个默认工作队列，可以由**多个内核组件和驱动程序共享使用**。
   - **自定工作队列** : 由**内核或驱动程序创建**特定工作队列，用于处理特定的任务。自定工作队列通常与特定的内核模块或驱动程序相关联，用于执行该模块或驱动程序相关的任务。

| 类别 | 共享工作队列                                                 | 自定工作队列                                 |
| ---- | ------------------------------------------------------------ | -------------------------------------------- |
| 优点 | `Linux` 自身就有，使用起来非常简单                           | 不会被其他工作影响                           |
| 缺点 | 该队列是共享的，因此会受到其他工作影响，导致任务长时间无法执行 | 工作队列需要自行创建，对系统会引起额外的开销 |

#### 延迟工作

1. 工作队列分为共享工作队列和自定义工作队列。**当我们把工作放入工作队列后，工作是稍后执行**。但我们存在一些应用场景，某个任务需要花费一定的时间，**任务并不需要马上执行，那么我们就可以使用延迟工作**。
2. **延迟工作不仅可以在自定义工作队列中实现，还可以在共享工作队列中实现**。
3. 任务可以在指定的延迟时间后执行，也可以根据优先级，任务类型或者其他条件进行排序和处理。
4. 延迟工作的应用场景 : 
   - 延迟工作常用于**处理那些需要花费较长时间的任务**，比如发送电子邮件，处理图像等。通过将这些任务放入队列中并延迟执行，可以**避免阻塞应用程序的主线程**，**提高系统的响应速度**。
   - 延迟工作可以**用来执行定时任务**，比如按键消抖后读取（不过需注意 `Linux` 中按键驱动是采用的软件定时器消抖）。通过将任务设置为在未来的某个时间点执行，**提高系统的可靠性和效率**。

#### 工作队列传参

1. 工作队列传参，本质上就是**利用的 `container_of` 宏，通过结构体成员的指针，反推出包含该成员的整个结构体的起始地址**。
2. 在如下队列中，我们申明一个结构体，然后正常的初始化该队列。之后当我们进入到中断下半段的时候，就可以利用  `container_of` 宏进行如下操作，得到我们想要获取的数值。最终实现工作队列的传参。

```c
struct work_data
{
  struct work_struct test_work;
  int a;
  int b;
};

struct work_data test_workqueue_work;
void test_work(struct work_struct *work)
{
  struct work_data *pdata;
  pdata = container_of(work, struct work_data, test_work);

  printk("a is %d", pdata->a);
  printk("b is %d", pdata->b);
}
```

#### 并发管理

1. 通过并发管理工作队列，我们可以同时处理多个任务或工作，提高系统的并发性和性能。每个任务独立地从队列中获取并执行，这种解耦使得整个系统更加高效、灵活，并且能够更好地应对多任务的需求。



### 中断线程化（`Interrupt Threading`）

1. **当中断处理需要在 `100us` 时间以上时，我们就应当使用下半部**。
2. **中断线程化比工作队列性能开销更小，不支持可延迟工作，灵活性弱通常与中断号绑定。而工作队列支持多队列，多并发等特性。**
3. 无论是**中断线程化**还是**工作队列**，都可能面临多线程环境下的**共享数据访问问题**。所以我们需要合理的进行同步操作，确保数据的一致性和准确性。
4. **部分中断不可以采取中断线程化，例如时钟中断**。有些流氓进程不主动让出处理器，内核只能依靠周期性的时钟中断夺回处理器的控制权，而时钟中断又是调度器的脉搏。因此，对于不可以线程化的中断，我们应当在注册函数的时候设置 `IRQF_NO_THREAD` 标志位。

## 中断号

1. 每个中断都有一个**软件中断号**（中断线，`IRQ Number`），通过中断号（中断线）可以区分不同中断，**在 Linux 内核中使用一个 int 变量表示中断号**。我们在驱动程序中利用中断号（中断线，`IRQ Number`）与指定的处理函数进行绑定。
2. 中断还有一个**硬件中断号**（`HW interrupt ID`），从驱动代码角度，我们只需要看中断号（中断线，`IRQ Number`）即可。但是**在设备树中配置的是硬件中断号（`HW interrupt ID`）**。所以在设备树中，我们需要设置**所属中断控制器**和 `IRQ` 号。

```dts
/{
    &uart1 {
        status = "okay";
        interrupts = <0 29 4>;  // 0 是中断控制器，29 是 IRQ 号，4 是触发方式
    };
};
```

3. 如果我们**只有一个中断控制器，软件中断号和硬件中断号是一一对应的**。但如果发生了如下这种**级联情况，那么就可能出现重复的硬件中断号**。如果 `CPU` 希望知道到底是那个中断事件，就还需要增加一个中断控制器的内容，通过**中断控制器**和**硬件中断号**配合在一起才可以知道到底是哪个中断。

![interrupt_id](./img/irq_id.png)

3. **将硬件中断号和软件中断号进行分离的好处是 : 一旦中断相关的硬件发生变化，驱动程序无需再进行调整**。Linux 内核会帮我们做硬件中断号和软件中断号的映射。

## 未处理中断/虚假中断

1. **虚假中断**是指系统检测到中断信号，但实际上并没有发生需要处理的中断事件。换句话说，系统进入了中断服务例程（ISR），但 ISR 找不到任何需要处理的实际任务。发生虚假中断的情况 : 
   - **信号干扰**：由于电气噪声或其他硬件故障，设备可能误发中断信号。
   - **共享中断线**：多个设备共用一个中断线（如 `PCI` 总线中的共享 `IRQ`），一个设备中断处理完成后可能导致中断信号没有及时清除，从而误触发其他设备的中断。
   - **中断控制器问题**：某些中断控制器（如 8259 PIC）在处理低优先级中断时，可能会错误地生成虚假中断信号。
2. 虚假中断会导致**CPU 占用率上升，性能下降**的问题。
3. **未处理中断**是指系统检测到中断信号并进入中断上下文，但没有找到对应的中断处理程序（ISR）来处理这个中断。结果，中断信号未能被清除，可能会重复触发。
   - **未注册中断处理程序**：设备驱动程序未能正确注册中断处理程序，导致中断信号无法被识别。
   - **中断号错误**：驱动程序请求了错误的中断号（`IRQ`），导致实际中断发生时找不到对应的处理程序。
   - **驱动程序错误**：驱动程序未正确清除中断状态，导致中断被多次重复触发。
4. 未处理中断会导致**设备不可用**、**系统崩溃**、**中断风暴（未清除的中断信号可能反复触发）**。
5. 为了处理如上问题，`Linux kernel` 做了对应的保护 : 如果同一个 `IRQ` 触发了 100,000 次，但是其中的 99,900 次没有被处理，那么 `kernel` 就会自己将这条中断线 `disable` 。

# 设备树

1. 对于板级 `BSP` 工程师，我们只需要了解 `interrupt-parent` 和 `interrupts` 这两个特性。
2. 而对于芯片原厂的 `BSP` 工程师，则需要编写 `interrupts` ，`interrupt-controller` ，`#interrupt-cells` 这三个参数。

## 属性介绍

### interrupt-parent

1. 该参数用于建立**中断信号源**与**中断控制器**之间关联的属性，表明当前中断信号源所属的中断控制器节点，用以确保中断的处理与分发。如下例子中，标识当前的 `key@1` 节点与中断控制器 `gpio5` 相连接。

```dts
key@1 {
    compatible = "dts_key@1";
    interrupt-parent = <&gpio5>;
    //...
};
```

![interrupt_devicetree](./img/irq_devicetree.png)

2. 如果 `interrupt-parent` 利用 `&` 包含了其他控制器，那么就会产生**级联**的效果。不过这仅需要了解即可，作为板级 `BSP` 工程师，**这些内容并不是由我们来编写，而是原厂工程师**。

```dts
intc: interrupt-controller@00a01000 {
    #interrupt-cells = <3>;
    //...
};

gpc: gpc@020dc000 {
    interrupt-controller;
    #interrupt-cells = <3>;
    // ...
};

soc {
    interrupt-parent = <&gpc>;
    //...
    aips1: aips-bus@02000000 {
        //...
        gpio5: gpio@020ac000 {
            interrupts = <GIC_SPI 74 IRQ_TYPE_LEVEL_HIGH>,
            <GIC_SPI 75 IRQ_TYPE_LEVEL_HIGH>;
            interrupt-controller;
            #interrupt-cells = <2>;
            //...
        };
    };
};
```

![interrupt_devicetree1](./img/irq_devicetree1.png)

### interrupts

1. `interrupts` 属性中每个中断的中断编号单元数量是由 `#interrupt-cells` 来进行指定的。但是如果你对比过多家三星，NXP，瑞芯微的**板级 `BSP` 设备树**编写会发现**格式都是统一**的。 `#interrupt-cells` 都是2.
2. 这个是不是 `Linux` 内核规定的我就不太清楚了。不过这几家芯片原厂的格式统一都是如下，`IRQ_number` 标识要用哪个中断号，`IRQ_trigger_type` 表示要使用触发中断的方式。

| 类型               | 含义                                                         |
| ------------------ | ------------------------------------------------------------ |
| `IRQ_number`       | 标识要用哪个中断号                                           |
| `IRQ_trigger_type` | 触发方式，拥有类型 : <br />- `IRQ_TYPE_NONE` : **不依赖外部信号触发**。可能是定时器触发，也可是软件触发<br />- `IRQ_TYPE_EDGE_RISING` : 上升沿触发<br />- `IRQ_TYPE_EDGE_FALLING` : 下降沿触发<br />- `IRQ_TYPE_EDGE_BOTH` : 双边沿触发<br />- `IRQ_TYPE_LEVEL_HIGH` : 高电平触发<br />- `IRQ_TYPE_LEVEL_LOW` : 低电平触发 |

```dts
// 格式
interrupts = <IRQ_number IRQ_trigger_type>;

// IRQ_trigger_type 的定义，在 ${kernel}/include/dt-bindings/interrupt-controller/irq.h
#define IRQ_TYPE_NONE           0
#define IRQ_TYPE_EDGE_RISING    1
#define IRQ_TYPE_EDGE_FALLING   2
#define IRQ_TYPE_EDGE_BOTH      (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)
#define IRQ_TYPE_LEVEL_HIGH     4
#define IRQ_TYPE_LEVEL_LOW      8


// 示例
gpio5: gpio@020ac000 {
            interrupt-controller;
            #interrupt-cells = <2>;
            //...
        };
key@1 {
    compatible = "dts_key@1";
    interrupt-parent = <&gpio5>;
    interrupts = <1 IRQ_TYPE_EDGE_FALLING>;
    //...
};
```

3. 但此时我们又会发现一个问题，为什么 `gpio5` 的 `interrupts` 属性有 3 个呢？这是因为它的父节点包含了 `gpc` ，而在 `gpc` 节点中 `#interrupt-cells = <3>;` 。不过这**些都不是我们板级 `BSP` 工程师要关心的事情了，这些工作应该是原厂工程师来做，我们简单了解即可**。

```dts
gpc: gpc@020dc000 {
    interrupt-controller;
    #interrupt-cells = <3>;
    // ...
};

soc {
    interrupt-parent = <&gpc>;
    //...
    aips1: aips-bus@02000000 {
        //...
        gpio5: gpio@020ac000 {
            interrupts = <GIC_SPI 74 IRQ_TYPE_LEVEL_HIGH>,
            <GIC_SPI 75 IRQ_TYPE_LEVEL_HIGH>;
            interrupt-controller;
            #interrupt-cells = <2>;
            //...
        };
    };
};
```

### #interrupt-cells

1. 通过上述 `interrupts` 属性的分析，我们也应该知道了 `#interrupt-cells` 属性作用。该属性就是用来描述每个中断信号源的中断编号单元的数量。
2. 一般来说，**不同层的 `#interrupt-cells` 为多少，`Linux` 都进行了规定和限制。作为原厂工程师和板级工程师我们只需要填坑即可**。

### interrupt-controller

1. `interrupt-controller` 属性用于**表示当前节点所描述的是一个中断控制器**。**该属性本身没有特定的属性值，只需要出现在属性列表中即可**。
2. 以下面为例子，我们存在 `gpio-controller;` 和 `interrupt-controller;` 两个属性。那么就表明**当前节点是中断控制器，同时也是一个 `GPIO` 控制器**。 

```dts
gpio1: gpio@0209c000 {
    compatible = "fsl,imx6ul-gpio", "fsl,imx35-gpio";
    reg = <0x0209c000 0x4000>;
    interrupts = <GIC_SPI 66 IRQ_TYPE_LEVEL_HIGH>,
    	<GIC_SPI 67 IRQ_TYPE_LEVEL_HIGH>;
    gpio-controller;
    #gpio-cells = <2>;
    interrupt-controller;
    #interrupt-cells = <2>;
};
```

## 获取属性信息 API

### irq_of_parse_and_map

1. 如果我们在设备树中配置 `interrupt-paren` 和 `interrupts`，那么我们就可以调用如下函数找到对应的中断号。
2. 这里需要注意，**一定要符合标准的 `interrupts = <IRQ_number IRQ_trigger_type>;` 属性写法！！！**

| 类型    | 描述                                                         |
| ------- | ------------------------------------------------------------ |
| 作用    | 解析设备树中的 `interrupts` 属性，并将对应的硬件中断号映射为软件中断号 |
| `node`  | 要进行解析的设备节点                                         |
| `index` | 索引号，要从 `interrupts` 属性中第几个中断获取中断号         |
| 返回值  | 成功时，返回一个映射后的软件中断号；失败返回0                |

```c
key@1 {
    compatible = "dts_key@1";
    interrupt-parent = <&gpio5>;
    interrupts = <1 IRQ_TYPE_EDGE_FALLING>;
    //...
};
unsigned int irq_of_parse_and_map(struct device_node *node, int index);
```

### irq_get_irq_data

1. 如果我们希望获取中断号的触发类型，就需要先调用该函数。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 获取与指定中断号（`IRQ`）相关的中断数据                      |
| `node` | 需要获取数据的中断号（`IRQ`），该值表示一个特定的中断资源    |
| 返回值 | 成功时，返回一个指向 `irq_data` 结构体的指针，表示该中断号的中断数据；失败返回 NULL |

```c
struct irq_data *irq_get_irq_data(unsigned int irq);
```

### irqd_get_trigger_type

1. 当我们获取了中断号（`IRQ`）相关的中断数据后，就可以调用该函数获取设备树中指定的中断类型了。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 获取获取指定中断的中断触发类型                               |
| `d`    | 要获取的中断触发类型的中断，传入一个指向 `irq_data` 结构体的指针 |
| 返回值 | 返回中断触发类型                                             |

```c
u32 irqd_get_trigger_type(struct irq_data *d);
```

2. 需要注意，**设备树中 `interrupts` 参数应当与 `request_irq` 中设置的中断触发类型一致，否则会可能出现如下报错 ！**因此我们需要使用当前的 `API` 获取设备树中设置的真实中断触发类型。

```shell
irq: type mismatch, failed to map hwirq-1 for /soc/aips-bus@02000000/gpio@020ac000!
```

### of_irq_get

1.  `of_irq_get()` 函数是对 `irq_of_parse_and_map` 的改进版，更适用于复杂或需要考虑延迟加载的场景。如果**你的应用场景涉及中断控制器可能存在延迟初始化问题，建议使用 `of_irq_get`**。 

```c
int of_irq_get(struct device_node *dev, int index)
{
	int rc;
	struct of_phandle_args oirq;
	struct irq_domain *domain;

	rc = of_irq_parse_one(dev, index, &oirq);
	if (rc)
		return rc;

	domain = irq_find_host(oirq.np);
	if (!domain)
		return -EPROBE_DEFER;

	return irq_create_of_mapping(&oirq);
}

unsigned int irq_of_parse_and_map(struct device_node *dev, int index)
{
	struct of_phandle_args oirq;

	if (of_irq_parse_one(dev, index, &oirq))
		return 0;

	return irq_create_of_mapping(&oirq);
}
```





# 中断 API 介绍

## 上半部

### gpio_to_irq

| 类型   | 描述                                                       |
| ------ | ---------------------------------------------------------- |
| 作用   | 将 `GPIO` 引脚编号转换成对应的中断请求号                   |
| `gpio` | 要映射的 `GPIO` 引脚号                                     |
| 返回值 | 成功时，返回与该 `GPIO` 引脚关联的 `IRQ` 编号;否则返回负数 |

```c
int gpio_to_irq(unsigned int gpio);
```

### request_irq/free_irq

1. 在 `Linux` 内核中，使用中断需要为其申请额外资源（如分配内存、初始化中断向量等），如果此时资源不可用，**`request_irq` 可能会睡眠，直到资源可用为止**。
2. 正因上面的原因，所以 `request_irq` 不能在**中断上下文**或者**其他禁止睡眠的代码段**中调用。
3. `request_irq` 是 Linux 内核中用于请求一个中断服务程序（Interrupt Service Routine, ISR）的函数，它**将一个中断号（IRQ）与一个指定的处理函数绑定**，以便在硬件中断发生时触发该处理函数。

| 类型      | 描述                                                         |
| --------- | ------------------------------------------------------------ |
| 作用      | 将中断号和指定处理函数进行绑定                               |
| `irq`     | 需要注册的中断号，用于标识中断源                             |
| `handler` | 指向中断处理程序的函数指针，当发生中断之后就会执行该函数     |
| `flags`   | 中断触发方式及其他配置，常见的标志 : <br /> - `IRQF_SHARED` : 共享中断。**多个设备共享一个中断线**，共享的所有中断都必须指定该标志，并且 `dev` 参数就是用于区分不同设备的唯一标识。<br />- `IRQF_ONESHOT` : **单次中断**，中断只执行一次。<br />- `IRQF_TRIGGER_NONE` : **不依赖外部信号触发**。可能是定时器触发，也可是软件触发。<br />- `IRQF_TRIGGER_RISING` : **上升沿触发**。<br />- `IRQF_TRIGGER_FALLING` : **下降沿触发**。<br />- `IRQF_TRIGGER_HIGH` : **高电平触发**。<br />- `IRQF_TRIGGER_LOW` : **低电平触发**。 |
| `name`    | 中断名称，用于标识中断，我们可以利用 `cat /proc/interrupts` 命令进行查看 |
| `dev_id`  | 如果 `flags` 被设置为了 `IRQF_SHARED`，该参数就是用于区分不同的中断。**该参数也将会被传递给中断处理函数 `irq_handler_t` 的第二个参数**。 |
| 返回值    | 申请成功返回0，否则为负数                                    |

```c
int request_irq(unsigned int irq, irq_handler_t handler,
                unsigned long flags, const char *name, void *dev);
```

4. 当我们需要注销之前通过 `request_irq()` 注册的中断处理程序，就可以调用如下函数。

| 类型     | 描述                                                         |
| -------- | ------------------------------------------------------------ |
| 作用     | 注销之前通过 `request_irq()` 注册的中断处理程序，并销毁中断注册时所产生的相关系统资源 |
| `irq`    | 需要注销的中断号                                             |
| `dev_id` | 如果该中断为共享中断，那么利用此参数区分具体中断，其值与 `request_irq()` 函数的最后一个参数 `dev_id` 一致。如果`request_irq()` 函数的最后一个参数 `dev_id` 传入值为 `NULL` ，那么这里也传入 `NULL` 即可。 |
| 返回值   | 无                                                           |

```c
void free_irq(unsigned int irq, void *dev_id);
```

### 中断服务程序（上半部）

| 类型     | 描述                                                         |
| -------- | ------------------------------------------------------------ |
| 作用     | `request_irq()` 注册的中断处理函数。**该函数为中断上半部**。 |
| `irq`    | 中断处理函数对应的中断号。因为**有可能多个中断共享一个中断处理函数，因此可以利用该参数进行区分不同的中断源**。 |
| `dev_id` | 与 `request_irq()` 函数的最后一个参数 `dev_id` 一致。**用于区分共享中断的不同设备，该参数也可以指向设备结构体**。 |
| 返回值   | 返回一个 `irqreturn_t` 类型参数，其内容如下 :<br />- `IRQ_NONE` : 中断未被处理<br />- `IRQ_HANDLED` : 中断已被成功处理<br />- `IRQ_WAKE_THREAD` : 还需要中断下半部进一步处理 |

```c
irqreturn_t handler(int irq, void *dev_id);

enum irqreturn {
	IRQ_NONE		= (0 << 0),
	IRQ_HANDLED		= (1 << 0),
	IRQ_WAKE_THREAD		= (1 << 1),
};

typedef enum irqreturn irqreturn_t;
```



## 下半部

### 任务（`Tasklet`）

#### DECLARE_TASKLET/DECLARE_TASKLET_DISABLED

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

2. 如果希望初始状态为非使能，那么就可以使用下面这个宏。

```c
#define DECLARE_TASKLET_DISABLED(name, func, data) \
struct tasklet_struct name = { NULL, 0, ATOMIC_INIT(1), func, data }
```

#### tasklet_init/tasklet_kill

1. `tasklet_init()` 可以对 `tasklet` 动态初始化，为软中断准备好响应的下半部处理机制，并不会触发中断。
2. `request_irq()` 注册硬件中断处理程序，**执行完成后，就立即开始响应硬件中断信息**，并调用注册的中断处理函数。为了避免可能的**逻辑问题**或**未初始化数据被使用**，通常建议**先调用 `tasklet_init()`** 初始化好对应的 `tasklet` 资源，**再调用 `request_irq()`** 注册硬件中断。
3. `tasklet_init` 注册的函数中，**不可以调用可能引起休眠的函数**。但如果是 `mdelay()` 这种**单纯消耗 `CPU` 进行阻塞的延时函数，那么是可行的**，不过不建议。

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

#### tasklet_schedule

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

#### tasklet_disable/tasklet_enable

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

### 软中断

#### open_softirq

1. 该函数为某一类型的软中断注册应用函数，设置软中断层的监听和处理策略。

| 类型     | 描述                                                         |
| -------- | ------------------------------------------------------------ |
| 作用     |                                                              |
| `nr`     | 软中断类型的标识，将软中断分为多个类型；指定将使用的软中断类型<br />- `HI_SOFTIRQ` : 高优先级中断<br />- `TIMER_SOFTIRQ` : 定时器软中断<br />- `NET_TX_SOFTIRQ` : 网络传输**发送**软中断<br />- `NET_RX_SOFTIRQ` : 网络传输**接收**软中断<br />- `BLOCK_SOFTIRQ` : 块设备软中断<br />- `IRQ_POLL_SOFTIRQ` : 支持 `IO` 轮询的块设备软中断<br />- `TASKLET_SOFTIRQ` : 低优先级的小任务<br />- `SCHED_SOFTIRQ` : 调度软中断，**用于处理器之间的均衡负载**<br />- `HRTIMER_SOFTIRQ` : 高精度定时器，但这种软中断**已经被废弃**，目前**中断处理程序的上半部处理高精度定时器**；保留的原因是当作**工具来保存编号**<br />- `RCU_SOFTIRQ` : `RCU` 软中断，**该中断最好是软中断的最后一个**<br />- `NR_SOFTIRQS` : 表示软中断的总数 |
| `action` | 处理软中断的函数；该函数接受一个 `struct softirq_action` 类型的参数 |
| 返回值   | 无                                                           |

```c
void open_softirq(int nr, void (*action)(struct softirq_action *));
```

#### raise_softirq

1. 触发软中断，一般在中断上半段调用。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 触发软中断                                                   |
| `nr`   | 要触发的软中断中断编号，与 `open_softirq()` 中注册的软中断类型一致 |
| 返回值 | 无                                                           |

```c
void raise_softirq(unsigned int nr);
```

#### raise_softirq_irqoff

1. **该函数必须在禁用硬中断的情况下调用**。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 触发软中断                                                   |
| `nr`   | 要触发的软中断中断编号，与 `open_softirq()` 中注册的软中断类型一致 |
| 返回值 | 无                                                           |

```c
void raise_softirq_irqoff(unsigned int nr)
```

### 共享工作队列

#### INIT_WORK/DECLARE_WORK

1. 在实际的驱动开发中，我们只需要定义工作项即可，关于工作队列和工作线程我们不需要去管。
2. 创建共享工作队列很简单，直接顶一个一个 `work_struct` 结构体变量，然后调用如下宏进行**动态初始化**即可。

| 类型    | 描述                             |
| ------- | -------------------------------- |
| 作用    | 初始化一个工作队列中的工作       |
| `_work` | 指向 `struct work_struct` 的指针 |
| `_func` | 工作项处理函数                   |
| 返回值  | 无                               |

```c
INIT_WORK(_work, _func);

// 驱动示例

struct work_struct test_workqueue; // 描述我们要延迟执行的工作

void test_work(struct work_struct *work)
{
  msleep(1000);
  printk("This is test_work\n");
}

irqreturn_t test_interrupt(int irq, void *args)
{
  // ...
  schedule_work(&test_workqueue);
  return IRQ_RETVAL(IRQ_HANDLED);
}

static int interrupt_irq_init(void)
{
    // ...
	INIT_WORK(&test_workqueue, test_work);
    request_irq(irq, test_interrupt, IRQF_TRIGGER_RISING, "test", NULL);
}
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

DECLARE_WORK(test_workqueue, test_work);

void test_work(struct work_struct *work)
{
  msleep(1000);
  printk("This is test_work\n");
}

irqreturn_t test_interrupt(int irq, void *args)
{
  // ...
  schedule_work(&test_workqueue);
  return IRQ_RETVAL(IRQ_HANDLED);
}

static int interrupt_irq_init(void)
{
    // ...
    request_irq(irq, test_interrupt, IRQF_TRIGGER_RISING, "test", NULL);
}
```

#### schedule_work

1. 将一个工作调度到共享工作队列中。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 将一个工作存放入系统工作队列中                               |
| `work` | 指向 `work_struct` 结构体的指针，工作项                      |
| 返回值 | 工作成功加入共享工作队列，返回 `true` ; 该工作已在共享工作队列，返回 `false` |

```c
bool schedule_work(struct work_struct *work);
```

#### cancel_work_sync

1. 该函数用于取消一个已经调度但尚未执行完成的工作。
2. 它可以确保目标工作要么已经完成执行，要么被成功取消，从而避免因工作正在执行导致的不确定行为。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 取消一个已经调度但尚未执行完成的工作                         |
| `work` | 向需要取消的 `struct work_struct` 的指针                     |
| 返回值 | 该工作已成功取消（工作未开始执行），返回 `true` ; 工作已经在执行，无法取消，返回 `false` |

```c
bool cancel_work_sync(struct work_struct *work);
```

### 自定义工作队列

#### create_workqueue/create_singlethread_workqueue/destroy_workqueue

1. 每个 `CPU` 都创建一个 `CPU` 相关的工作队列。因此**自定义工作队列资源消耗大，如果不是需要隔离和特殊配置的工作，建议还是采用共享内存**。
2. 它提供了比共享工作队列（如 `system_wq`）更大的灵活性，适合需要特殊配置或隔离的场景，可以**避免任务间的互相干扰**。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 每个 `CPU` 都创建一个 `CPU` 相关的工作队列                   |
| `name` | 工作队列的名称                                               |
| 返回值 | 成功，返回一个指向新创建的 `struct workqueue_struct` 的指针；失败，返回 `NULL` |

```c
struct workqueue_struct *create_workqueue(const char *name);
```

3. 只给一个 `CPU` 创建一个 `CPU` 相关的工作队列。

4. 该函数可能存在性能限制。因为绑定在一个 `CPU` 上执行，可能导致另外的 `CPU` 长期处于空闲状态。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 只给一个 `CPU` 创建一个 `CPU` 相关的工作队列                 |
| `name` | 工作队列的名称                                               |
| 返回值 | 成功，返回一个指向新创建的 `struct workqueue_struct` 的指针；失败，返回 `NULL` |

```c
create_singlethread_workqueue(const char *name);
```

5. 当我们创建了自定义工作队列，并且不再想使用后，可以调用如下函数进行销毁。

| 类型   | 描述                                                  |
| ------ | ----------------------------------------------------- |
| 作用   | 销毁自定义工作队列                                    |
| `wq`   | 需要销毁的工作队列指针（`struct workqueue_struct *`） |
| 返回值 | 无                                                    |

```c
void destroy_workqueue(struct workqueue_struct *wq);
```

#### queue_work_on/flush_workqueue/cancel_work_sync

1. 当我们创建好了一个自定义工作队列以后，将需要进行延迟的工作放在工作队列上，需要调用如下函数。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 销毁自定义工作队列                                           |
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

2. 当我们希望加入工作队列的任务快点执行，我们可以调用如下函数。一般该函数**多用于销毁自定义队列之前，确保自定义队列被安全的销毁**。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 等待一个工作队列中的所有已提交的工作项完成执行，**该函数将会引起阻塞** |
| `wq`   | 需要被执行的工作队列指针（`struct workqueue_struct *`）      |
| 返回值 | 无                                                           |

```c
void flush_workqueue(struct workqueue_struct *wq);
```

3. 要取消一个已经调度的工作，如果被取消的工作已经正在执行，则会等待他执行完成再返回。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 要取消一个已经调度的工作                                     |
| `wq`   | 需要被取消的工作队列指针（`struct workqueue_struct *`）      |
| 返回值 | 任务成功被取消，返回 `true` ; 任务未被取消（可能已经执行或从未被排入队列），返回 `false` |

```c
bool cancel_work_sync(struct work_struct *work);
```

### 延迟工作

#### DECLARE_DELAYED_WORK/INIT_DELAYED_WORK

1. **静态定义**并初始化延迟工作队列。

| 类型            | 描述                                                         |
| --------------- | ------------------------------------------------------------ |
| 作用            | 静态定义并初始化一个延迟工作队列                             |
| `work_name`     | 延迟工作变量名，最终产生一个名字相同的 `intel_cqm_rmid_work *` 结构体指针变量 |
| `work_function` | 需要延迟工作的处理函数                                       |
| 返回值          | 无                                                           |

```c
DECLARE_DELAYED_WORK(work_name, work_function);
```

2. 当我们希望**动态定义**并初始化延迟工作队列，可以使用如下宏。
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

#### schedule_delayed_work/queue_delayed_work/cancel_delayed_work_sync

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

3. 如果需要取消调度，就应该使用如下函数。**无论是共享工作队列还是自定义工作队列，这个 `API` 都能正确地取消该工作项**。

| 类型    | 描述                                                         |
| ------- | ------------------------------------------------------------ |
| 作用    | 动态分配在运行时初始化的延迟工作                             |
| `dwork` | 指向一个 `struct delayed_work` 的指针或实例，表示要取消调度的延迟工作项 |
| 返回值  | 成功取消返回 `true` ，否则返回 `false`                       |

```c
bool cancel_delayed_work_sync(struct delayed_work *dwork);
```

### 并发管理

#### alloc_workqueue

1. 我们可以利用如下函数**代替创建自定义工作队列**的 `API` ，该 `API` 将会创建一个新的**自定义工作队列**（`struct workqueue_struct`），并根据指定的参数配置工作队列的行为，例如**并发性**、**是否绑定到 `CPU`** 等。

| 类型         | 描述                                                         |
| ------------ | ------------------------------------------------------------ |
| 作用         | 创建一个自定义工作队列                                       |
| `fmt`        | 工作队列的名称格式字符串                                     |
| `flags`      | 指定工作队列的属性和行为，常见的可选值包括：<br />- `WQ_UNBOUND` : 创建一个无绑定的工作队列，可以在任何 `COU` 上运行；适合需要并发或跨 `CPU` 执行的任务，此类西任务执行时间较长，需要充分利用系统多核资源；虽然这种方法可以**增加并发性**，但同时也**增加了调度的复杂性**<br />- `WQ_FREEZABLE` : 与**电源管理相关，表示当前工作队列可被冻结**<br />- `WQ_MEM_RECLAIM` : 表示**当前队列可能用于内存回收**，这意味着当系统内存不足时，当前工作队列可能会被**优先处理，用以帮助释放内存资源**；一般用于**内存回收**、**文件系统**写回场景<br />- `WQ_HIGHPRI` : 表示**高优先级工作队列**<br />- `WQ_CPU_INTENSIVE` : 表示该工作队列用于 `CPU` 密集型任务，一般用于**特别耗时**的工作 |
| `max_active` | 控制工作队列的并发性，即允许同时运行的任务数量；如果设置为 0 ，表示使用默认值 |
| 返回值       | 成功取消返回 `true` ，否则返回 `false`                       |

```c
struct workqueue_struct *alloc_workqueue(const char *fmt, unsigned int flags, int max_active);
```

### 中断线程化（推荐）

#### request_threaded_irq

1. 对于**不需要延迟工作**的中断下半部，**中断线程化会比工作队列更合适**，因为**工作队列实时性没有中断线程化高**。

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

# 调试

## debug
### 硬中断

1. 当 `gpio` 作为外部中断使用时，调用 `cat /proc/interrupts` 命令可以查看**所有硬中断的相关信息** :
   - `CPU0` : 表示在哪个 `CPU`  核心上处理。
   - 16 : 软件中断号（`IRQ Number`）。**软件中断号是唯一的**，内核通过这个标识找到对应的中断处理程序。
   - 106214 : 中断号对应的中断在 `CPU0` 上被触发的总次数。例如，中断号 16 被触发了 106,214 次。
   - `GPC` : 表示中断控制器的类型。在这里，`GPC` 是一个中断控制器的名字，可能与特定硬件平台相关。它负责将硬件中断请求映射到内核。
   - 55 : 表示硬件中断信号（`HW interrupt ID`），这是特定平台中断控制器的内部编号。
   - `Level` : 触发类型。`Level` 表示电平触发，`Edge` 表示边沿触发。
   - `i.MX Timer Tick` : 表示中断对应的功能或设备。常见的设备名称包括定时器、串口、SPI 控制器等。在这里，`i.MX Timer Tick` 是一个定时器中断，用于系统时钟调度。

```shell
$ cat /proc/interrupts 
           CPU0       
 16:     106214       GPC  55 Level     i.MX Timer Tick
 18:          0       GPC  31 Level     2008000.ecspi
 19:          0       GPC  33 Level     2010000.ecspi
 20:        315       GPC  26 Level     2020000.serial
```

### 软中断

1. `cat /proc/soft` 可以查看各种**软中断**在不同的 `CPU` 上累计的运行次数。

```shell
$ cat /proc/softirqs 
                    CPU0       
          HI:          3
       TIMER:       1424
      NET_TX:         10
      NET_RX:        137
       BLOCK:          0
    IRQ_POLL:          0
     TASKLET:       1446
       SCHED:          0
     HRTIMER:          0
         RCU:       2568
```

## 性能优化

### 亲缘绑定

1. `/proc/irq/` 目录中存放着**以软件中断号命名的文件夹**，每个中断号文件夹下都有几个节点，这些节点存放对应中断号的信息。我们一般看 **`smp_affinity` 节点，该节点代表中断号与 `CPU` 之间的亲缘绑定关系**。如果某个中断号绑定了一个 `CPU` 核，那么这个中断就会一直在这个 `CPU` 上处理。利用该节点，我们可以让性能进行优化，使得空闲 `CPU` 忙起来。

```shell
$ cat /proc/irq/44/smp_affinity
1
```

2. 我们可以通过串口对该参数进行赋值，不过重启之后就会消失。
3. 我们也可以**通过 `irq_set_affinity()` 函数指定中断掩码，最终实现将某个中断被固定到指定的 `CPU` 运行**。

### 均衡负载

1. 当我们调用 `cat /proc/soft` 查询软中断在不同的 `CPU` 执行情况时，在不同的 `CPU` 上累计应当差不多。

## 中断唤醒系统

1. `Linux` 内核 



# 面试题

## 上下文的概念是什么

1. 上下文是**指程序或者系统当前所处的执行环境和状态**，它包括了**程序的状态、寄存器值、内存映射、文件描述符、信号处理器等信息**。上下文是一个抽象的概念，它描述了程序或者系统的执行环境和运行状态。
2. **上文（Context of the Process/Thread Before）** : **用于保存当前任务运行状态**，操作系统需要保存的全部寄存器值、程序计数器（PC）、堆栈指针等。
3. **下文（Context of the Process/Thread After）** : **恢复任务时的状态信息，通常是从保存的“上文”中恢复的内容**。

## Linux 中存在几种上下文

2. 在 `Linux` 中通常分为**中断上下文**和**进程上下文**。
2. **中断上下文**又分为**硬中断上下文**和**软中断上下文**。
3. 一个进程的上下文可以分为三个部分 : **用户级上下文**、**寄存器上下文**以及**系统级上下文**。

   - **用户级上下文** : 正文、数据、用户堆栈以及共享存储区；
   - **寄存器上下文** : 通用寄存器、程序寄存器(`IP`)、处理器状态寄存器(`EFLAGS`)、栈指针(`ESP`)；
   - **系统级上下文** : 进程控制块 `task_struct`、内存管理信息(`mm_struct`、`vm_area_struct`、`pgd`、`pte`)、内核栈。

## 中断上下文和进程上下文的区别

1. 

## 中断上半部、下半部关系

5. **同一个中断上半部和下半部是多对一的关系**。
2. 因为中断下半部是可以被中断的，当中断**刚执行到步骤7**时，**同样的中断**又发生了一次，那么就会跳转到步骤1。然后执行到步骤4，发现 `preempt_count` 不是0，那么**第二次中断的中断下半部不会被执行，而是返回恢复中断线程，也就是第一次中断的终端下半部**。

![interrupt_flow](./img/irq_flow.png)

## 为什么中断上下文不允许休眠

2. **所谓睡眠就是调用 `schedule` 函数让出 `CPU`** ，使其执行其他任务，**这个过程涉及到进程栈空间的切换**。
3. 如果是进程A发生中断，而在中断上下文中又调用 `schedule` 函数进行了调度，变成进程B。那么此时**进程A的上下文信息将会被进程B的内容覆盖，从而导致无法回到进程A**。
3. 因为中断上半部是处于关中断状态。当我们调用睡眠函数时，**睡眠函数会调用 `schedule` 函数让出 `CPU`**，进而去执行其他进程。**而中断一直处于关闭状态，无法 Linux 内核无法进行正常的调度行为，从而系统崩溃**。

## 为什么进程上下文可以休眠

1. 

## 中断上下文有那些注意事项

1. **不能调用可能引起阻塞等待的 `API`** 。
2. **不能尝试获取信号量**。因为信号量会引起睡眠。
3. **不能执行耗时的任务**。因为中断上下文是禁用中断，如果长时间处于中断上下文，会导致内核大量的服务和请求无法被响应，严重影响系统功能。
4. **不能访问虚拟内存**。因为中断在内核空间。

## 用户态到内核态的几种方式

1. **系统调用** : 当我们执行 `open()` 、`read()`、 `write()`**操作文件**，调用 `fork()` **创建进程**，`malloc()` **分配内存**都将会进行用户态到内核态的转变。
2. **中断** : 触发中断，操作系统会暂时中断当前执行的用户态程序，转而处理该中断请求。
3. **异常** : 异常触发时，处理器会将程序的执行转移到操作系统内核态，操作系统会进行处理，比如抛出信号、终止进程等。

## 为什么中断号会稀疏，不连续情况

1. 如果只有**一个 `CPU` ，一个中断控制器**，我们可以保证物理中断信号到虚拟中断号的映射是线性的，这样可以使用**数组**进行表示并没有问题。
2. 但如果是**多个 `CPU` ，多个中断控制器**，就没有办法保证虚拟中断信号是连续的，因此需要使用到**基数树**。
   - 在多 CPU 和多中断控制器的情况下，不同中断控制器管理的中断号可能存在以下问题：
     - **非连续映射**：每个中断控制器的中断号范围可能不同，并且在多个控制器之间可能存在“稀疏”分布。例如：控制器 A 管理中断号 `0-31`。控制器 B 管理中断号 `100-131`。之间存在较大间隙
     - **动态分配**：稀疏分布导致**无法预先知道要获取的内存**。如果使用**数组**可能存在**大量空槽，浪费空间和查找效率**。
   - **基数树**是一种高效的数据结构，特别**适合稀疏键空间**。它通过**路径压缩**和**前缀匹配机制**，解决了中断号稀疏和不连续的问题。
     - **节省存储空间**：基数树只存储有效的中断号路径，不用为空闲的中断号分配存储空间。
     - **支持稀疏键空间**：基数树能够高效地存储和查找稀疏分布的中断号。
     - **灵活性**：基数树支持动态插入和删除中断号映射，非常适合动态分配中断号的场景。

## 硬中断、软中断、信号区别

1. **硬中断** : 外部设备对 `CPU` 的中断。
2. **软中断** : 中断下半部的一种处理机制。
3. **信号** : 内核（其他进程）对某个进程的中断。
4. **软中断和基于软中断的 `tasklet` 如果出现短时间的井喷现象， `Linux` 内核会吧后续软中断放入 `ksoftirqd` 内核线程中执行**。
5. 优先级 : 硬中断 > 软中断 > 线程
6. 软中断适度线程化，可以缓解高负载情况下的系统响应。

## 为什么中断上半部不能调用 disable_irq

1. `disable_irq()` 函数可能会造成阻塞。如果该函数是想要禁止当前中断号的中断行为，而又在当前中断中调用该函数，将会造成**死锁**。
2. 因为 `disable_irq()` 函数会等待中断处理程序执行完成再进行禁止，而我们却在中断中调用 `disable_irq()` 函数，最终程序将会卡死。

