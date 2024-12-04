# GPIO 操作函数

## 查看 GPIO 使用信息

```shell
cat /sys/kernel/debug/gpio
```



## GPIO 子系统

### 注册与注销

1. 注册 GPIO。

```c
/* 作用 : 申请一个指定编号的 GPIO，确保该 GPIO 当前未被其他驱动或代码占用，并为后续的 GPIO 操作做准备
 * gpio  : GPIO 的编号
 * label : 用于标识该 GPIO 的字符串，便于调试和跟踪
 * 返回值 : 申请失败返回负数，成功返回 0
 */
int gpio_request(unsigned gpio, const char *label);
```

2. 注销 GPIO

```c
/* 作用 : 注销 GPIO
 * gpio : 需要注销的 GPIO 编号
 */
void gpio_free(unsigned int gpio);
```

### GPIO 方向

1. 将 GPIO 设置为输出方向。

```c
/* 作用 : 将指定的 GPIO 配置为输出模式，并设置其初始输出电平
 * gpio  : 要配置为输出模式的 GPIO 编号
 * value : 输出的初始电平，0为低电平，非0为高电平(通常为1)
 * 返回值 : 操作失败返回负数，成功返回 0
 */
int gpio_direction_output(unsigned int gpio, int value);
```

2. 将 GPIO 设置为输入方向。

```c
/* 作用 : 将指定的 GPIO 配置为输入模式
 * gpio  : 要配置为输入模式的 GPIO 编号
 * 返回值 : 操作失败返回负数，成功返回 0
 */
int gpio_direction_input(unsigned int gpio);
```

### GPIO 控制

1. 在 GPIO 被配置为输出模式时，可以通过该函数控制其电平状态。

```c
/* 作用 : 设置指定 GPIO 的输出电平
 * gpio  : 需要设置电平的 GPIO 编号
 * value : 0为低电平，非0为高电平(通常为1)
 */
void gpio_set_value(unsigned int gpio, int value);
```

2. 读取指定 GPIO 的输出电平状态。

```c
/* 作用 : 读取指定 GPIO 的电平状态
 * gpio  : 需要读取电平状态的 GPIO 编号
 * 返回值 : 0为低电平，1为高电平
 */
int gpio_get_value(unsigned int gpio);
```

##  sysfs 操作 

### 利用 sysfs 点亮一个 LED

```shell
echo 131 > /sys/class/gpio/export            # 注册 GPIO 131，与 gpio_request() 函数作用类似
echo out > /sys/class/gpio/gpio131/direction # 将 GPIO 131 设置为输出模式,与 gpio_direction_output() 函数作用类似
echo 1 > /sys/class/gpio/gpio131/value       # 将 GPIO 131 设置为高电平,与 gpio_set_value(132,1) 函数作用类似
echo 131 > /sys/class/gpio/unexport          # 释放 GPIO 131 引脚的控制权限,与 gpio_free() 函数类似
```

### 利用 sysfs 设置一个按键

```shell
echo 129 > /sys/class/gpio/export            # 注册 GPIO 131，与 gpio_request() 函数作用类似
echo in > /sys/class/gpio/gpio129/direction  # 将 GPIO 131 设置为输入模式,与 gpio_direction_input() 函数类似
cat /sys/class/gpio/gpio129/value            # 读取 GPIO 131 引脚的当前值,与 gpio_get_value() 函数类似
echo 129 > /sys/class/gpio/unexport          # 释放 GPIO 131 引脚的控制权限,与 gpio_free() 函数类似
```

