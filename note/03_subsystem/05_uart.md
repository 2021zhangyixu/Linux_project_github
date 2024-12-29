# 基础概念

1. 线路规程可以分流，将其转流到 蓝牙、WiFi。
2. early printk 是开机打印的串口
3. 流控用于解决速度不同或者是缓冲区已满。一般不开流控，因为可能出现我想要 log 但是不会继续打印。
4. **普通开发人员不需要写串口驱动程序，直接写应用程序即可**。
5. 在 Linux 系统中，串口设备通常会以 `ttyS`、`ttyUSB`、`ttyACM` 等形式出现，具体取决于串口硬件类型。
   - **`ttyS`**：通常对应传统的 UART 串口，如 8250 兼容串口。
   - **`ttyUSB`**：用于 USB 转串口的设备（如 USB 转 RS-232 转换器）。
   - **`ttyACM`**：通常用于 USB 调制解调器（例如 GSM、3G 调制解调器等）
6. 使用串口之前，需要使能串口，教程参考：https://www.bilibili.com/video/BV1fJ411i7PB?spm_id_from=333.788.player.switch&vd_source=a6289e7ab435b598d918e64d45cd15c0&p=89

# API





# debug 

/proc/tty/driver 可以查看tx发送的字节数，rx接收的字节数量

/proc/tty/drivers

/proc/tty/ldisc 查看线路规程