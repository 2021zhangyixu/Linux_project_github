# 平台总线模型

1. 平台总线是 Linux 系统虚拟出来的总线。平台总线将一个驱动分为两个部分一个 `driver.c` 控制硬件和 一个 `device.c`描述硬件。
2. **平台总线是通过字符串比较，将 `name `相同的 `driver.c` 和 `device.c`匹配到一起**。
3. 平台总线的原则是先分离，后搭档。

##  platform_device 结构体介绍

1. 当我们注册一个 `platform_device` 结构体，其中的 `name` 将会在 `/sys/bus/platform/devices/`中，后面的数字是成员变量 `id`。`id`用于区分设备，如果设置为-1，表示不要后缀。如果使能了 `id_auto`变量，那么 `id`设置的值将会被覆盖，也就是说，**内核优先自动分配 `id`，而忽略用户显式设置的 `id`**。不过我们一般不使用 `id_auto`变量。
   - `name = "test";id = -1;id_auto = false;` : sys/bus/platform/devices/test
   - `name = "test";id = 1;id_auto = false;` : sys/bus/platform/devices/test.1
   - `name = "test";id = 1;id_auto = true;` : sys/bus/platform/devices/test.0.auto
2. `platform_device` 中的 `dev` 成员表示设备的通用属性部分。**在编写 device 部分代码的时候，必须实现 `dev.release` 中的函数，否则会报错**。
3. `resource` 成员是最核心的内容，用于描述硬件相关信息。如果 `flags` 表示是要描述内存类型的资源，`.start` 为寄存器的起始地址，`.end` 为寄存器的结束地址。如果是描述中断号，GPIO 编号，那么 `.start` 和 `.end` 都写一样的值。

```c
#define MEM_START_ADDR 0xFEC30004
#define MEM_END_ADDR   0xFEC30008
#define IRQ_NUMBER     112

static struct resource my_resources[] = {
    {
        .start = MEM_START_ADDR,    // 内存资源起始地址
        .end = MEM_END_ADDR,        // 内存资源结束地址
        .flags = IORESOURCE_MEM,    // 标记为内存资源
    },
    {
        .start = IRQ_NUMBER,        // 中断资源号
        .end = IRQ_NUMBER,          // 中断资源号
        .flags = IORESOURCE_IRQ,    // 标记为中断资源
    },
};
static struct platform_device my_platform_device = {
    .num_resources = ARRAY_SIZE(my_resources),     // 资源数量
    .resource = my_resources,                      // 资源数组
    // ...
};
```

4. 代码中，我们只需要调用如下两个代码进行注册和卸载。当我们卸载驱动的时候， `dev.release` 中的函数首先会被调用，然后调用 `module_exit` 中的函数。

```c
/* 作用 : 注册一个 platform_device 实例，将其添加到平台设备管理中
 * pdev : 指向待注册平台设备的指针
 * 返回值 : 如果注册成功，返回 0，否则返回负数
 */
int platform_device_register(struct platform_device *pdev);

/* 作用 : 注销一个平台设备，并释放与该设备相关联的资源
 * pdev : 指向待注销平台设备的指针
 */
void platform_device_unregister(struct platform_device *pdev);
```

## platform_driver 结构体介绍

1. 在驱动程序中，我们需要添加一个 `platform_driver` 结构体用于获取设备信息。**当 `platform_driver`  发现总线上存在与自己`name` 一样的 `platform_device`  时候，将会自动触发 `probe` 函数**。于是我们可以在 `probe` 函数中注册字符设备和 GPIO 信息。
2. `platform_driver` 中的 `name` 将会在 `/sys/bus/platform/drivers/`中。
3. 当我们需要在驱动代码中加入平台总线模型，只需要加入如下代码即可。将原来 `module_init` 和 `module_exit` 的函数放到 `probe` 和 `remove` 中。

```c
static struct platform_driver led_driver = {
	.driver		= {
		.owner = THIS_MODULE,
		.name	= "led_device",
	},
	.probe		= led_drver_probe,
	.remove		= led_drver_remove,
};

static int __init chr_drv_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&led_driver);
	if(ret<0)
	{
		printk("platform_driver_register error \n");
		return ret;
	}
	printk("platform_driver_register ok \n");
    return 0;
}

static void __exit chr_drv_exit(void)
{
	platform_driver_unregister(&led_driver);
	printk("platform_driver_unregister ok \n");
}
```

4. `platform_driver`  结构体中的 `id_table` 成员表示设备的列表，这个 `id_table` 中的 `name` 也可以用来匹配。 **`id_table` 中的 `name` 匹配优先级要大于 `driver` 中的 `name`** 。

5. 还需要注意，`platform_driver`  结构体中的 **`driver_override` 成员变量表示成员变量用于强制选择特定的驱动程序来匹配平台设备**。**如果该成员变量被设置，那么就只有一条匹配规则**，如果匹配成功执行 `probe` ，否则就直接退出。**但是如果没有设置该变量，那么那么他会匹配设备树，匹配平台总线**。

## 驱动获取设备信息

### 直接获得资源数组

`platform_driver` 结构体继承了 `platform_device ` 结构体，因此可以直接访问 `platform_device ` 中定义的成员。**不过不建议使用这种办法，因为设备程序将会和驱动程序耦合过高**。

```c
// device.c
static struct resource my_resources[] = {
    {
        .start = MEM_START_ADDR,    // 内存资源起始地址
        .end = MEM_END_ADDR,        // 内存资源结束地址
        .flags = IORESOURCE_MEM,    // 标记为内存资源
    },
    {
        .start = IRQ_NUMBER,        // 中断资源号
        .end = IRQ_NUMBER,          // 中断资源号
        .flags = IORESOURCE_IRQ,    // 标记为中断资源
    },
};
static struct platform_device my_platform_device = {
    .num_resources = ARRAY_SIZE(my_resources),     // 资源数量
    .resource = my_resources,                      // 资源数组
    // ...
};

// driver.c
static int my_platform_driver_probe(struct platform_device *pdev)
{
    struct resource *res_mem, *res_irq;

    // 方法1：直接访问 platform_device 结构体的资源数组
    if (pdev->num_resources >= 2) {
        struct resource *res_mem = &pdev->resource[0];
        struct resource *res_irq = &pdev->resource[1];

        // 使用获取到的硬件资源进行处理
        printk("Method 1: Memory Resource: start = 0x%llx, end = 0x%llx\n",
                res_mem->start, res_mem->end);
        printk("Method 1: IRQ Resource: number = %lld\n", res_irq->start);
    }    struct resource *res_mem, *res_irq;

    // 方法1：直接访问 platform_device 结构体的资源数组
    if (pdev->num_resources >= 2) {
        struct resource *res_mem = &pdev->resource[0];
        struct resource *res_irq = &pdev->resource[1];

        // 使用获取到的硬件资源进行处理
        printk("Method 1: Memory Resource: start = 0x%llx, end = 0x%llx\n",
                res_mem->start, res_mem->end);
        printk("Method 1: IRQ Resource: number = %lld\n", res_irq->start);
    }
}
```

### 通过 API 接口获得

1. `type` 类型就是在 `platform_device ` 中设置的 `flags`，`num` 就是在同一个类型下第几个资源。

```c
/* 作用 : 在 Linux 内核中用于获取与平台设备相关的资源
 * pdev : 一个指向 platform_device 结构体的指针，表示平台设备
 * type : 资源的类型，用于指定你想获取的资源类型
 * num  : 表示你要获取的资源的索引，如果设备有多个资源（例如多个内存区域或多个中断线），你可以通过 num 来指定具体的资源索引
 * 返回值 : 如果成功返回一个指向 struct resource 结构体的指针，表示设备资源的详细信息，失败返回 NULL
 */
struct resource *platform_get_resource(struct platform_device *pdev, unsigned int type, unsigned int num);
```

2. 在 Linux 中没有直接获得相同类型设备资源数量的方法。所以目前推荐的方法有如下两种。

```c
// 假设 device 都是相同类型的资源，那么我们可以直接利用如下方式获取
int count = pdev->num_resources;

// 如果 device 中包含各种类型的资源，那么我们只能通过 while 循环的方式获取
int count;
struct resource *led_resource;
while (1) {
    led_resource = platform_get_resource(pdev, IORESOURCE_IRQ, count);
    if (led_resource) {
        count++;
        //...
    } else {
        break;
    }
}
```

3. 对于设备 device 中的 name 进行获取方式。

```c
struct resource *res_irq;
res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, i);
sprintf(s_char_drv.gpios[i].name, "%s", res_irq->name);
```

