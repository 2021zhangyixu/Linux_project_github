# 基础概念

1. pinctrl 本质上就是对引脚的复用能力进行配置，例如某个 GPIO 存在 4 种工作模式。那么我们就需要使用 pinctrl 来切换工作模式。
2. 当我把一个引脚配置为 GPIO 功能，那么就可以使用 GPIO 子系统来进行配置。
3. pinctrl 作用：
   - 获取设备树中的 pin 信息
   - 获取到的 pin 信息设置 pin 的复用功能（工作模式）
   - 获取到的 pin 信息来设置 pin 的电气特性，例如 上/下拉，速度，驱动能力（驱动电流）等
4. 对于使用者而言，**只需要在设备树中设置好某一个 pin 的相关属性即可，其他初始化工作均由 pinctrl 子系统完成**。
5. pinctrl 子系统的驱动程序是由原厂开发人员了解，该子系统帮助普通驱动开发人员管理芯片引脚并自动完成引脚初始化。普通驱动开发人员只需要在设备树中按照规定的格式写出想要配置的参数即可。
6. 芯片的可用引脚（出去电源引脚和特定功能引脚）是有限的，芯片原厂为了提高硬件设计的灵活性，一个引脚需要有多个片上外设。

# API 

## devm_pinctrl_get

1. 获取设备管理器中获取一个设备的引脚控制句柄。
2. `devm_` 起始的 API 代表了 **设备管理的自动化资源释放**。这些 API 的特点是，内核会自动处理资源的释放，避免了开发人员需要手动释放资源的麻烦。

| 类型   | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 作用   | 从设备管理器中获取一个设备的引脚控制句柄                     |
| `dev`  | 指向设备的指针，表示引脚控制器与哪个设备关联                 |
| 返回值 | 成功时，返回一个指向 `struct pinctrl` 的指针，表示该设备的引脚控制器；失败返回 `NULL` |

```c
struct pinctrl *devm_pinctrl_get(struct device *dev);

// 驱动示例
static int led_drver_probe(struct platform_device *pdev)
{
    struct pinctrl *pinctrl;
    pinctrl = devm_pinctrl_get(&pdev->dev);
    if (!pinctrl || IS_ERR(pinctrl)) {
        dev_err(&pdev->dev, "can't get pinctrl, bus recovery not supported\n");
        return PTR_ERR(pinctrl);
    }
    // ...
}
```

## pinctrl_lookup_state

1. 根据指定的状态名称查找引脚控制器的引脚配置状态。

| 类型         | 描述                                                         |
| ------------ | ------------------------------------------------------------ |
| 作用         | 根据指定的状态名称查找引脚控制器的引脚配置状态               |
| `pinctrl`    | 指向 `struct pinctrl` 的指针，表示要查找状态的引脚控制器；由 `devm_pinctrl_get()` 函数获取的返回值 |
| `state_name` | 字符串，表示要查找的状态名称。需要与设备树的 `pinctrl-name` 对应 |
| 返回值       | 成功时，返回指向 `struct pinctrl_state` 的指针，表示找到的引脚配置状态；失败返回 `NULL` |

```c
struct pinctrl_state *pinctrl_lookup_state(struct pinctrl *pinctrl, const char *state_name);

// 设备树
&iomuxc {
   pinctrl-names = "default","sleep","init";
   pinctrl-0 = <&pinctrl_uart1>;
   pinctrl-1 =<&xxx>;
   pinctrl-2 =<&yyy>;
...
   pinctrl_uart1: uart1grp {
      fsl,pins = <
         MX6UL_PAD_UART1_TX_DATA__UART1_DCE_TX 0x1b0b1
         MX6UL_PAD_UART1_RX_DATA__UART1_DCE_RX 0x1b0b1
      >;
   };

   xxx: xxx_grp {
      ...这里设置将引脚设置为其他模式
   }

   yyy: yyy_grp {
      ...这里设置将引脚设置为其他模式
   }

...
}
// 驱动示例
static int led_drver_probe(struct platform_device *pdev)
{
    struct pinctrl *pinctrl;
    struct pinctrl_state *status_default,status_sleep,status_init;
        
    pinctrl = devm_pinctrl_get(&pdev->dev);
    if (!pinctrl || IS_ERR(pinctrl)) {
        dev_err(&pdev->dev, "can't get pinctrl, bus recovery not supported\n");
        return PTR_ERR(pinctrl);
    }
    status_default = pinctrl_lookup_state(pinctrl,"default");
    status_sleep   = pinctrl_lookup_state(pinctrl,"sleep");
    status_init    = pinctrl_lookup_state(pinctrl,"init");
}
```

## pinctrl_select_state

1. 切换到指定的引脚配置状态。这会修改引脚的电气状态和功能，使其处于指定的工作模式（例如，从默认状态切换到低功耗模式）。

| 类型      | 描述                                                         |
| --------- | ------------------------------------------------------------ |
| 作用      | 切换引脚状态                                                 |
| `pinctrl` | 指向 `struct pinctrl` 的指针，表示要查找状态的引脚控制器；由 `devm_pinctrl_get()` 函数获取的返回值 |
| `state`   | 指向 `struct pinctrl_state` 的指针，表示要应用的引脚状态；由 `pinctrl_lookup_state()` 函数获取的返回值 |
| 返回值    | 成功时，返回 0，表示状态切换成功；失败返回负数，错误码       |

```c
int pinctrl_select_state(struct pinctrl *pinctrl, struct pinctrl_state *state);

// 设备树
&iomuxc {
   pinctrl-names = "default","sleep","init";
   pinctrl-0 = <&pinctrl_uart1>;
   pinctrl-1 =<&xxx>;
   pinctrl-2 =<&yyy>;
...
   pinctrl_uart1: uart1grp {
      fsl,pins = <
         MX6UL_PAD_UART1_TX_DATA__UART1_DCE_TX 0x1b0b1
         MX6UL_PAD_UART1_RX_DATA__UART1_DCE_RX 0x1b0b1
      >;
   };

   xxx: xxx_grp {
      ...这里设置将引脚设置为其他模式
   }

   yyy: yyy_grp {
      ...这里设置将引脚设置为其他模式
   }

...
}
// 驱动示例
static int led_drver_probe(struct platform_device *pdev)
{
    struct pinctrl *pinctrl;
    struct pinctrl_state *status_default,*status_sleep,*status_init;
        
    pinctrl = devm_pinctrl_get(&pdev->dev);
    if (!pinctrl || IS_ERR(pinctrl)) {
        dev_err(&pdev->dev, "can't get pinctrl, bus recovery not supported\n");
        return PTR_ERR(pinctrl);
    }
    status_default = pinctrl_lookup_state(pinctrl,"default");
    status_sleep   = pinctrl_lookup_state(pinctrl,"sleep");
    status_init    = pinctrl_lookup_state(pinctrl,"init");
    
    pinctrl_select_state(pinctrl, status_default);
    mdelay(1000);
    pinctrl_select_state(pinctrl, status_sleep);
    mdelay(1000);
    pinctrl_select_state(pinctrl, status_init);
    mdelay(1000);
}
```



# 设备树

1. 在设备树中，分为**客户端**和**服务端**两部分。客户端的写法，所有平台都是一样的。服务端会根据芯片平台不同有所区别。

## 客户端

1. 如下为客户端一些内容的定义 :
   - pinctrl-names : 定义消费者，默认配置为 “default”，其他名字可以自定义
   - pinctrl-0 : 对应第0种状态的需要使用到的引脚配置信息，可引用其他节点
   - pinctrl-1 : 对应第1种状态的需要使用到的引脚配置信息，可引用其他节点
   - pinctrl-2 : 对应第2种状态的需要使用到的引脚配置信息，可引用其他节点

```dts
&iomuxc {
   pinctrl-names = "default","sleep","init";
   pinctrl-0 = <&pinctrl_uart1>;
   pinctrl-1 =<&xxx>;
   pinctrl-2 =<&yyy>;
...
   pinctrl_uart1: uart1grp {
      fsl,pins = <
         MX6UL_PAD_UART1_TX_DATA__UART1_DCE_TX 0x1b0b1
         MX6UL_PAD_UART1_RX_DATA__UART1_DCE_RX 0x1b0b1
      >;
   };

   xxx: xxx_grp {
      ...这里设置将引脚设置为其他模式
   }

   yyy: yyy_grp {
      ...这里设置将引脚设置为其他模式
   }

...
}
```

## 服务端

1. 服务端根据芯片平台的定义，情况有所不同，需要在 `${linux}/Documentation/devicetree/bindings/pinctrl` 目录下查看。

### imx

1. pinctrl 驱动程序是通过读取 `fsl,pins` 属性值来获取 pin 的匹配信息的。因此 `fsl,pins` 语法是什么，是根据不同的芯片厂商来规定的。如果是
2. 服务端的标签（label）必须以 `pinctrl_` 开头，整个标签（label） 长度不能超过 32 字节。 

```dts
// 服务端，不同的芯片平台有区别
// imx6ull
pinctrl_自定义名字: 自定义名字 {
    fsl,pins = <
            引脚复用宏定义   PAD（引脚）属性
            引脚复用宏定义   PAD（引脚）属性
    >;
};
```

4. 在 `imx` 芯片平台中 `fsl,pins` 本质上有 6 个参数，但是因为 PAD 配置很灵活，所以一般都采用数字。关于 PAD 的配置信息，我们可以看 `${linux}/Documentation/devicetree/bindings/pinctrl/fsl,imx6ul-pinctrl.txt` 文档。

- SW_MUX_CTL_PAD*寄存器：用于设置某个引脚的IOMUX，选择该引脚的功能；
- SW_PAD_CTL_PAD*寄存器：用于设置某个引脚的属性，比如驱动能力、是否使用上下拉电阻等；

4. 在 引脚复用宏定义 的宏中，本质上其实是5个数，内容如下。

   - mux_reg 与 iomuxc 外设寄存器的及地址 reg = <0x020e0000 0x4000>; 配合可以得到实际要操控的寄存器。
   - mux_mode 为被操控寄存器的内容。
   -  conf_reg ，引脚（PAD）属性控制寄存器偏移地址。
   - **input_reg** 和 **input_val** ，input_reg暂且称为输入选择寄存器偏移地址。input_val是输入选择寄存器的值。 这个寄存器只有某些用作输入的引脚才有。

```dts
<mux_reg    conf_reg    input_reg   mux_mode    input_val>

#define PAD_CTL_HYS                     (1 << 16)  // 使能施密特触发器（Hysteresis）
#define PAD_CTL_PUS_100K_DOWN           (0 << 14)  // 100K下拉电阻
#define PAD_CTL_PUS_47K_UP              (1 << 14)  // 47K上拉电阻
#define PAD_CTL_PUS_100K_UP             (2 << 14)  // 100K上拉电阻
#define PAD_CTL_PUS_22K_UP              (3 << 14)  // 22K上拉电阻
#define PAD_CTL_PUE                     (1 << 13)  // 使能内部上拉/下拉
#define PAD_CTL_PKE                     (1 << 12)  // 使能保持器（Keeper）功能
#define PAD_CTL_ODE                     (1 << 11)  // 使能开漏输出
#define PAD_CTL_SPEED_LOW               (0 << 6)   // 低速（50MHz）
#define PAD_CTL_SPEED_MED               (1 << 6)   // 中速（100MHz）
#define PAD_CTL_SPEED_HIGH              (3 << 6)   // 高速（200MHz）
#define PAD_CTL_DSE_DISABLE             (0 << 3)   // 禁用驱动强度
#define PAD_CTL_DSE_260ohm              (1 << 3)   // 驱动强度：260欧姆
#define PAD_CTL_DSE_130ohm              (2 << 3)   // 驱动强度：130欧姆
#define PAD_CTL_DSE_87ohm               (3 << 3)   // 驱动强度：87欧姆
#define PAD_CTL_DSE_65ohm               (4 << 3)   // 驱动强度：65欧姆
#define PAD_CTL_DSE_52ohm               (5 << 3)   // 驱动强度：52欧姆
#define PAD_CTL_DSE_43ohm               (6 << 3)   // 驱动强度：43欧姆
#define PAD_CTL_DSE_37ohm               (7 << 3)   // 驱动强度：37欧姆
#define PAD_CTL_SRE_FAST                (1 << 0)   // 快速信号上升沿
#define PAD_CTL_SRE_SLOW                (0 << 0)   // 缓慢信号上升沿

```

### rk

1. 瑞芯微的 pinctrl 的服务端写法如下 :
   - PIN_BANK : PIN 所属组，在 `${linux}/arch/arm/boot/dts/include/dt-bindings/pinctrl/rockchip.h` 中有可看到对应宏定义。如果我们希望设置 `GPIO0_C0` 这个 PIN，那么 PIN_BANK 就是 RK_GPIO0。
   - PIN_BANK_IDX : 组内编号，以 `GPIO0` 组为例，一共有 `A0~A7` 、 `B0~B7` 、 `C0~C7` 、 `D0~D7` ，这 32 个 PIN。分别是是 `#define RK_PA0 0` 、 `#define RK_PA5 5` 、 `#define RK_PD2 26` 。
   - MUX : PIN 的**复用功能**，一个 PIN 最多 16 个复用功能。其宏定义为 `RK_FUNC_*`。具体的复用信息需要查看芯片手册。
   - phandle : 描述**引脚配置**信息。例如上拉、下拉等信息。

```dts
rockchip,pins = <PIN_BANK PIN_BANK_IDX MUX &phandle>
```



# 问题

1. 明明使用的是 gpio 功能，但 pin 脚工作模式不对，或者明明输出高电平，实际输出低电平。
2. 原因是可能是因为公司规模太大，在项目迭代，上一个项目的相关 pin 脚工作模式控制代码没有删除导致有问题。
3. 全局搜 gpio number 关键字，是否可以搜到
4. 如果某个 pin 脚永远无法配置，可能是**被系统安全模式锁住，找芯片原厂**。
5. 内存监控，检测 GPIO 的寄存器什么时候被修改。
6. 在 API 中加 debug log 查看。
7. 多核异构处理器，CPU 核间通讯可能会篡改寄存器，导致修改失败。