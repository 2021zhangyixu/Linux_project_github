# 基础概念

1. 按键、鼠标、键盘、触摸屏等都属于 input 设备，Linux 内核为此做了一个 input 子系统。
2. input 子系统本质上也就是一个**字符设备**。用户只需要负责上报输入事件，例如按键值，坐标等信息，input 核心层负责处理该事件。
3. input 子系统中所有设备的**主设备号都是 13**。在使用 input 子系统处理输入设备时，就**无需再去注册字符设备，只需要向系统注册一个 input_device 即可**。





# API 

## 注册/销毁上报事件

```c
input_allocate_device
input_free_device
    
input_register_device
input_unregister_device
    
```

## 上报事件

```c
input_event
input_sync
```

