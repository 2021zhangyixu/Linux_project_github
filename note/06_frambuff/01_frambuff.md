# 基础概念

1. 对于 LCD 显示屏，原厂已经给我们适配好了，我们只需要调整一下设备树的内容即可。

```dts
&lcdif {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_lcdif_dat
             &pinctrl_lcdif_ctrl
             &pinctrl_lcdif_reset>; 
    display = <&display0>;
    status = "okay";
    reset-gpios = <&gpio3 4 GPIO_ACTIVE_LOW>; /* 100ask */

    display0: display {
        bits-per-pixel = <24>; /* 每个像素点 24 bit，也就采用 RGB888，如果是32就是ARGB8888 */
        bus-width = <24>; /* 数据线长度，24根 */

        display-timings {
            native-mode = <&timing0>;

             timing0: timing0_1024x768 {
             clock-frequency = <50000000>; /* 时钟频率，单位 HZ */
             hactive = <1024>;             /* 水平显示区域 */
             vactive = <600>;              /* 垂直显示区域 */
             hfront-porch = <160>;         /* HFP */
             hback-porch = <140>;          /* HBP */
             hsync-len = <20>;             /* HSPW */
             vback-porch = <20>;           /* VBP */
             vfront-porch = <12>;          /* VFP */
             vsync-len = <3>;              /* VSPW */
             hsync-active = <0>;           /* 可选参数，水平同步信号的有效电平。低电平有效 */
             vsync-active = <0>;           /* 可选参数，垂直同步信号的有效电平。低电平有效 */
             de-active = <1>;              /* 可选参数，数据使能信号的有效电平。高电平有效 */
             pixelclk-active = <0>;        /* 可选参数，像素时钟的有效电平。低电平有效 */
             };

        };
    };
};
```

2. 如下节点为屏幕设备。

```shell
/dev/fbx
```

3. 输入如下命令可以让屏幕打印一串  `hello linux` 。

```shell
 echo hello linux > /dev/tty1
```

