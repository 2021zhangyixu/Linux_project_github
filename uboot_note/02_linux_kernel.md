# 基础概念

1. PID 0 进程为主进程，主进程后续会退化成 idle 进程。

# 测试 kernel

1. 利用如下命令将 NXP 官方维护的 Linux kernel 进行解压缩。
2. 使用如下命令进行测试一次

```shell
make clean
make imx_v7_mfg_defconfig
make -j18
```

3. 在开发板中执行如下，从 Ubuntu 下载镜像和 dtb 文件。

```shell
tftp 80800000 zImage
tftp 83000000 imx6ull-14x14-evk-emmc.dtb
bootz 80800000 - 83000000
```

4. 之后我们可以根据 SMP 后的内容，看到 kernel 编译时间用于确认是否为最新编译的内核镜像。（需要注意，这个时间是**编译环境的时间**）

```shell
Starting kernel ...

Booting Linux on physical CPU 0x0
Linux version 4.1.15 (book@100ask) (gcc version 7.5.0 (Buildroot 2020.02-gee85cab) ) #4 SMP PREEMPT Wed Jan 8 20:22:31 EST 2025
```

# 添加自己的开发板

## 添加 defconfig

1. 添加我们自己的配置文件

```shell
cd ${linux_kernel_PATH}/arch/arm/configs
cp imx_v7_defconfig imx_sailing_14x14_emmc_defconfig
```

## 添加 dts文件

1. 复制设备树文件，并修改 Makefile。

```shell
cd ../boot/dts
cp imx6ull-14x14-evk.dts imx6ull-sailing-14x14-emmc.dts
```

2. 编译测试

```shell
make clean
make imx_sailing_14x14_emmc_defconfig
make -j18
```

## 配置根文件系统

1. 在 `imx6ull-14x14-evk-emmc.dts` 中将如下内容复制到 `imx6ull-sailing-14x14-emmc.dts` 中并进行替换。

```c
&usdhc2 {
	pinctrl-names = "default", "state_100mhz", "state_200mhz";
	pinctrl-0 = <&pinctrl_usdhc2_8bit>;
	pinctrl-1 = <&pinctrl_usdhc2_8bit_100mhz>;
	pinctrl-2 = <&pinctrl_usdhc2_8bit_200mhz>;
	bus-width = <8>;
	non-removable;
	status = "okay";
};
```

2. 编译，并加载到开发板中。

```shell
make dtbs
```

3. 配置根文件系统，将根文件系统存放在 EMMC 的分区 2 里面，命令如下。

```shell
setenv bootargs 'console=ttymxc0,115200 root=/dev/mmcblk1p2 rootwait rw'
saveenv
```

## 修改主频

1. 查看 cpu 信息

```shell
cat /proc/cpuinfo
```

2. 执行如下命令查看 cpu 工作频率。

```shell
cd /sys/bus/cpu/devices/cpu0/cpufreq/
cat cpuinfo_cur_freq
```

3. 我们进入 menuconfig 的如下目录，可以设置 CPU 的调频策略。

```shell
→ CPU Power Management → CPU Frequency scaling → Default CPUFreq governor
```

4. CPU 超频可能带来风险，请自行决定。参考正点原子 34.4.1.2章节，或者视频 13.2 节24min

## 修改 EMMC 驱动

1. Linux 内核 EMMC 默认采用 4 线模式，而我们当前开发板采用的是 8 线的，因此需要在  `imx6ull-sailing-14x14-emmc.dts` 中将 `bus-width` 设置为 8。
2. 因为我们的 EMMC 的工作电压是 3.3V ，为了防止内核采用 1.8V 电压驱动 EMMC，因此需要加上 `no-1-8-v;` 。

```shell
&usdhc2 {
	pinctrl-names = "default", "state_100mhz", "state_200mhz";
	pinctrl-0 = <&pinctrl_usdhc2_8bit>;
	pinctrl-1 = <&pinctrl_usdhc2_8bit_100mhz>;
	pinctrl-2 = <&pinctrl_usdhc2_8bit_200mhz>;
	bus-width = <8>;
	non-removable;
	no-1-8-v;
	status = "okay";
};
```

## 修改网络驱动

### 设备树

1. 参考如下内容，对应引脚进行注释。

```dts
pinctrl_spi4: spi4grp {
                        fsl,pins = <
                                MX6ULL_PAD_BOOT_MODE0__GPIO5_IO10        0x70a1
                                MX6ULL_PAD_BOOT_MODE1__GPIO5_IO11        0x70a1
                                /*MX6ULL_PAD_SNVS_TAMPER7__GPIO5_IO07      0x70a1
                                MX6ULL_PAD_SNVS_TAMPER8__GPIO5_IO08      0x80000000 */
                        >;
                };
                
spi4 {
		/* pinctrl-assert-gpios = <&gpio5 8 GPIO_ACTIVE_LOW>; */
		/* cs-gpios = <&gpio5 7 0>; */
		// ... 
}
```

2. 找到 `iomuxc_snvs` 节点添加如下内容。

```dts
 &iomuxc_snvs {
	pinctrl-names = "default_snvs";
    pinctrl-0 = <&pinctrl_hog_2>;
    imx6ul-evk {
    	// ...
        /*enet1 reset */
        pinctrl_enet1_reset: enet1resetgrp {
            fsl,pins = <
                /* used for enet1 reset */
                MX6ULL_PAD_SNVS_TAMPER7__GPIO5_IO07 0x10B0 
            >;
        };
        /*enet2 reset */
        pinctrl_enet2_reset: enet2resetgrp {
            fsl,pins = <
                /* used for enet2 reset */
                MX6ULL_PAD_SNVS_TAMPER8__GPIO5_IO08 0x10B0 
            >;
    	};
    };
}
```

3. 将如下两个引脚的电气属性进行修改。

```dts
		pinctrl_enet1: enet1grp {
			fsl,pins = <
				// ...
				/* MX6UL_PAD_ENET1_TX_CLK__ENET1_REF_CLK1	0x4001B009 */
				MX6UL_PAD_ENET1_TX_CLK__ENET1_REF_CLK1 0x4001b031
			>;
		};

		pinctrl_enet2: enet2grp {
			fsl,pins = <
				//...
				/* MX6UL_PAD_ENET2_TX_CLK__ENET2_REF_CLK2	0x4001B009 */
				MX6UL_PAD_ENET2_TX_CLK__ENET2_REF_CLK2 0x4001b031
			>;
		};
```

4. 修改 fec1 和 fec2 的 pinctrl-0 属性

```dts
pinctrl-0 = <&pinctrl_enet2 
		     &pinctrl_enet2_reset>;
```



5. 找到如下节点，添加 `smsc,disable-energy-detect;` 表明 PHY 芯片是 SMSC 公司，这样 Linux 内核就会找到 SMSC 公司的 PHY 芯片来驱动 LAN8720A。

```shell
&fec2 {
	// ...

		ethphy0: ethernet-phy@2 {
			compatible = "ethernet-phy-ieee802.3-c22";
			smsc,disable-energy-detect;
			reg = <2>;
		};

		ethphy1: ethernet-phy@1 {
			compatible = "ethernet-phy-ieee802.3-c22";
			smsc,disable-energy-detect;
			reg = <1>;
		};
	};
};
```

### 驱动代码

1. 进入 `drivers/net/ethernet/freescale/fec_main.c`  在 `fec_reset_phy` 函数的最后一行加上一个 150ms 以上的延时。

```c
#ifdef CONFIG_OF
static void fec_reset_phy(struct platform_device *pdev)
{
	int err, phy_reset;
	int msec = 1;
	struct device_node *np = pdev->dev.of_node;

	if (!np)
		return;

	err = of_property_read_u32(np, "phy-reset-duration", &msec);
	/* A sane reset duration should not be longer than 1s */
	if (!err && msec > 1000)
		msec = 1;

	phy_reset = of_get_named_gpio(np, "phy-reset-gpios", 0);
	if (!gpio_is_valid(phy_reset))
		return;

	err = devm_gpio_request_one(&pdev->dev, phy_reset,
				    GPIOF_OUT_INIT_LOW, "phy-reset");
	if (err) {
		dev_err(&pdev->dev, "failed to get phy-reset-gpios: %d\n", err);
		return;
	}
	msleep(msec);
	gpio_set_value(phy_reset, 1);
	/* sr8021f takes 150ms to operate */
	msleep(200);
}
```

### 测试

1. 进入开发板的 uboot 阶段，将刚编译好的文件拷贝下来

```shell
# Ubuntu
make clean
make imx_sailing_14x14_emmc_defconfig
make -j18
# 开发板
tftp 80800000 zImage
tftp 83000000 imx6ull-sailing-14x14-emmc.dtb
bootz 80800000 - 83000000 
```

2. 手动设置 ip 信息，并进行 ping，如果成功表示配置完成。

```shell
ifconfig eth0 192.168.5.9 netmask 255.255.255.0 up
ping 192.168.5.11
```



