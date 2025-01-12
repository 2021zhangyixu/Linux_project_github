# 基础概念

1. SD 卡存在三个分区：第一个分区存储 uboot、第二个分区是存储 Linux 内核和设备树、第三个分区存储根文件系统。
2. 当使用 USB 启动，插上 OTG 线，片上 ROM 在 USB 模式下会模拟出一个 6ull 一般设备。



# 官方uboot

## 解压编译

1. 作为非原厂的 BSP 开发人员，我们都是基于原厂开发板进行的适配。因此我们需要找到原厂的 BSP 包进行修改。
2. 找到原厂的 BSP 包后，执行如下命令解压

```shell
tar -vxjf uboot-imx-rel_imx_4.1.15_2.1.0_ga.tar.bz2
```

3. 解压后进入目录，执行如下命令即可看到 nxp 官方的 evk 开发板的默认配置。

```shell
$ ls configs/mx6ull_14x14_evk*
configs/mx6ull_14x14_evk_defconfig       configs/mx6ull_14x14_evk_nand_defconfig
configs/mx6ull_14x14_evk_emmc_defconfig  configs/mx6ull_14x14_evk_qspi1_defconfig
```

4. 执行如下命令进行配置，因为我是韦东山的虚拟机，在 `~/.bashrc` 中已配置了相关环境，因此直接执行无需再设置 ARCH 等环境

```shell
make distclean
# 这里 14x14 是指芯片封装大小为 14mm*14mm
make mx6ull_14x14_evk_emmc_defconfig
make -j16
```

## 需要调整的内容

1. uboot 能够正常启动，串口正常。
2. LCD 显示屏需要根据自己的进行调整。
3. 网络找不到。
4. sd 卡和 emmc 驱动检查

```shell
$ mmc list
# 查看设备信息
$ mmc info
# 切换设备
$ mmc dev 1
```

# uboot 修改

## 添加自己开发板

### 添加板子默认文件

1. 进入 configs 目录，将官方的 EVK 开发板配置文件复制一份。

```shell
cd ${uboot_PATH}/configs
cp mx6ull_14x14_evk_emmc_defconfig mx6ull_sailing_14x14_emmc_defconfig
```

2. 执行如下命令测试一次文件是否正常。

```shell
cd ${uboot_PATH}
make distclean
make mx6ull_sailing_14x14_emmc_defconfig 
make -j16
ls u-boot.*
```

3. 修改 `mx6ull_sailing_14x14_emmc_defconfig` 文件内容

```
CONFIG_SYS_EXTRA_OPTIONS="IMX_CONFIG=board/freescale/mx6ull_sailing_14x14_emmc/imximage.cfg,MX6ULL_EVK_EMMC_REWORK"
CONFIG_ARM=y
CONFIG_ARCH_MX6=y
CONFIG_TARGET_MX6ULL_SAILING_14X14_EMMC=y
CONFIG_CMD_GPIO=y
```

### 添加头文件

1. 在 `include/configs/` 目录下添加自己开发板对应的头文件。

```shell
cd ${uboot_PATH}
cp include/configs/mx6ullevk.h include/configs/mx6ull_sailing_14x14_emmc.h
```

2. 修改 `mx6ull_sailing_14x14_emmc.h` 文件内容

```h
// 修改之前
#ifndef __MX6ULLEVK_CONFIG_H
#define __MX6ULLEVK_CONFIG_H
// 修改后
#ifndef __MX6ULL_SAILING_14x14_EMMC_CONFIG_H
#define __MX6ULL_SAILING_14x14_EMMC_CONFIG_H
```

### 添加开发板板级文件夹

1. 执行如下命令复制一份板级文件，并修改当前目录下一些文件名字。

```shell
cd ${uboot_PATH}
cp board/freescale/mx6ullevk board/freescale/mx6ull_sailing_14x14_emmc -r
cd board/freescale/mx6ull_sailing_14x14_emmc
mv mx6ullevk.c mx6ull_sailing_14x14_emmc.c  
```

2. 修改 Makefile 的目标编译文件。

```makefile
obj-y  := mx6ull_sailing_14x14_emmc.o
```

3. 这里需要参考当前章节第一步 `cp board/freescale/mx6ullevk board/freescale/mx6ull_sailing_14x14_emmc -r` 的文件夹命名。

```cfg
// 修改前
PLUGIN	board/freescale/mx6ullevk/plugin.bin 0x00907000
// 修改后
PLUGIN	board/freescale/mx6ull_sailing_14x14_emmc/plugin.bin 0x00907000
```

4. 修改 Kconfig 文件。
   - TARGET_MX6ULL_SAILING_14X14_EMMC : 这里内容参考 **添加板子默认文件 ** 章节的 `mx6ull_sailing_14x14_emmc_defconfig`  文件中 `CONFIG_TARGET_MX6ULL_SAILING_14X14_EMMC=y` 内容，并将前面的 `CONFIG_` 去掉。
   - mx6ull_sailing_14x14_emmc : 随便写，建议为自己板子命名信息。
   - mx6ull_sailing_14x14_emmc : 板子名字，随便写，建议为自己板子命名信息。

```Kconfig
// 修改前
if TARGET_MX6ULL_14X14_EVK || TARGET_MX6ULL_9X9_EVK
// 修改后
if TARGET_MX6ULL_SAILING_14X14_EMMC

// 修改前
config SYS_BOARD
	default "mx6ullevk"
// 修改后
config SYS_BOARD
	default "mx6ull_sailing_14x14_emmc"

// 修改前
config SYS_CONFIG_NAME
	default "mx6ullevk"
// 修改后
config SYS_CONFIG_NAME
	default "mx6ull_sailing_14x14_emmc"

```

5. 修改 MAINTAINERS 文件为如下内容。

```
MX6ULL_SAILING BOARD
M:	Zhang YiXu <zhangyixu02@gmail.com>
S:	Maintained
F:	board/freescale/mx6ull_sailing_14x14_emmc/
F:	include/configs/mx6ull_sailing_14x14_emmc.h
F:	configs/mx6ull_sailing_14x14_emmc_defconfig
```

6. 修改 `${uboot_PATH}/arch/arm/cpu/armv7/mx6/Kconfig` 文件。
   - TARGET_MX6ULL_SAILING_14X14_EMMC : 这里内容参考 **添加板子默认文件 ** 章节的 `mx6ull_sailing_14x14_emmc_defconfig`  文件中 `CONFIG_TARGET_MX6ULL_SAILING_14X14_EMMC=y` 内容，并将前面的 `CONFIG_` 去掉。
   - board/freescale/mx6ull_sailing_14x14_emmc/Kconfig : 这里参考当前章节第一步 `cp board/freescale/mx6ullevk board/freescale/mx6ull_sailing_14x14_emmc -r` 的文件夹命名。

```Kconfig
// 在 TARGET_MX6ULL_9X9_EVK 后面追加
config TARGET_MX6ULL_SAILING_14X14_EMMC
	bool "Support mx6ull_sailing_14x14_emmc"
	select MX6ULL
	select DM
	select DM_THERMAL
// 文件最后面追加
source "board/freescale/mx6ull_sailing_14x14_emmc/Kconfig"
```

### 测试

1. 执行如下命令查看，第一次执行 `ls u-boot.*` 无法看到相关文件，第二次执行有。

```shell
make distclean 
make mx6ull_sailing_14x14_emmc_defconfig
ls u-boot.*
make -j16
ls u-boot.*
```

2. 第一条命令能看到相关 .o 文件，而第二条不可以。

```shell
ls board/freescale/mx6ull_sailing_14x14_emmc/*.o
ls board/freescale/mx6ullevk/*.o
```

3. 执行如下命令能够看到很多文件对该 .h 文件进行了引用。

```shell
grep "mx6ull_sailing_14x14_emmc.h" -nwr
```

## 修改屏幕

1. 进入 `${sailing_uboot_4.1.15_2.1.0}/board/freescale/mx6ull_sailing_14x14_emmc/mx6ull_sailing_14x14_emmc.c`修改内容：
   - `pixclock` 成员表示像素时钟周期的长度，单位为皮秒（ps）。`pixclock` 的值等于像素时钟频率的倒数，再乘以 10¹²，以将秒转换为皮秒。

```c
struct display_info_t const displays[] = {{
	.bus = MX6UL_LCDIF1_BASE_ADDR,
	.addr = 0,
	.pixfmt = 24,
	.detect = NULL,
	.enable	= do_enable_parallel_lcd,
	.mode	= {
		.name			= "TFT7016",
		.xres           = 1024,
		.yres           = 600,
		.pixclock       = 19531,
		.left_margin    = 140,
		.right_margin   = 160,
		.upper_margin   = 20,
		.lower_margin   = 12,
		.hsync_len      = 20,
		.vsync_len      = 3,
		.sync           = 0,
		.vmode          = FB_VMODE_NONINTERLACED
} } };
```

2. 将 `${sailing_uboot_4.1.15_2.1.0}/include/configs/mx6ull_sailing_14x14_emmc.h` 中的 panle 参数，与上述的 `.name` 一致。
3. 后续 Uboot 启动，LCD 上将会出现一个 NXP 标志。

## 修改网络

1. 根据 IEEE 802.3 标准，地址为 0 至 15 的前 16 个寄存器的功能被统一定义，称为基本控制和状态寄存器。这些寄存器在不同厂商的 PHY 芯片中具有相同的功能和定义。地址为 16 至 31 的寄存器则留给芯片制造商自行定义，用于实现特定功能。
2. 因为 PHY 芯片的前 16 个寄存器被统一定义，因此 U-Boot 提供了一个通用的 PHY 驱动框架，能够处理标准寄存器的基本操作。

### 修改 PHY 地址

1. `${sailing_uboot_4.1.15_2.1.0}/include/configs/mx6ull_sailing_14x14_emmc.h` 文件内容

```h


// SR8201F
#define CONFIG_PHYLIB
// #define CONFIG_PHY_MICREL
#define CONFIG_PHY_REALTEK
#endif
```

### 删除 74LV595

1. 屏蔽 `${sailing_uboot_4.1.15_2.1.0}/board/freescale/mx6ull_sailing_14x14_emmc/mx6ull_sailing_14x14_emmc.c` 中如下代码。并加上相应代码

```c
// #define IOX_SDI IMX_GPIO_NR(5, 10)
// #define IOX_STCP IMX_GPIO_NR(5, 7)
// #define IOX_SHCP IMX_GPIO_NR(5, 11)
// #define IOX_OE IMX_GPIO_NR(5, 8)

#define ENET1_RESET IMX_GPIO_NR(5, 7)
#define ENET2_RESET IMX_GPIO_NR(5, 8)

// static iomux_v3_cfg_t const iox_pads[] = {
// 	/* IOX_SDI */
// 	MX6_PAD_BOOT_MODE0__GPIO5_IO10 | MUX_PAD_CTRL(NO_PAD_CTRL),
// 	/* IOX_SHCP */
// 	MX6_PAD_BOOT_MODE1__GPIO5_IO11 | MUX_PAD_CTRL(NO_PAD_CTRL),
// 	/* IOX_STCP */
// 	MX6_PAD_SNVS_TAMPER7__GPIO5_IO07 | MUX_PAD_CTRL(NO_PAD_CTRL),
// 	/* IOX_nOE */
// 	MX6_PAD_SNVS_TAMPER8__GPIO5_IO08 | MUX_PAD_CTRL(NO_PAD_CTRL),
// };
```

2. 注释 `iox74lv_init()` 函数。
3. 在 `board_init()` 函数中注释 `iox74lv_init();` 和 `imx_iomux_v3_setup_multiple_pads(iox_pads, ARRAY_SIZE(iox_pads));` 。

2. 添加开发板网络复位引脚驱动。fec1_pads 用于网络1，fec2_pads 描述网络2。

```c
static iomux_v3_cfg_t const fec1_pads[] = {
    // ...
	MX6_PAD_SNVS_TAMPER7__GPIO5_IO07 | MUX_PAD_CTRL(NO_PAD_CTRL),
}
static iomux_v3_cfg_t const fec1_pads[] = {
    // ...
	MX6_PAD_SNVS_TAMPER7__GPIO5_IO07 | MUX_PAD_CTRL(NO_PAD_CTRL),
}
```

5. 找到如下函数进行替换。

```c
static void setup_iomux_fec(int fec_id)
{
	if (fec_id == 0)
	{
		imx_iomux_v3_setup_multiple_pads(fec1_pads,
						 ARRAY_SIZE(fec1_pads));

		gpio_direction_output(ENET1_RESET, 1);
		gpio_set_value(ENET1_RESET, 0);
		mdelay(20);
		gpio_set_value(ENET1_RESET, 1);
	}
	else
	{
		imx_iomux_v3_setup_multiple_pads(fec2_pads,
						 ARRAY_SIZE(fec2_pads));

		gpio_direction_output(ENET2_RESET, 1);
		gpio_set_value(ENET2_RESET, 0);
		mdelay(20);
		gpio_set_value(ENET2_RESET, 1);
	}
	mdelay(150); // 如果是 SR8201F 需要加上这个延时
}
```

### 配置网络信息

1. 打开uboot后可以看到如下内容

```shell
Net:   FEC1
Error: FEC1 address not set.
```

2. 后续需要配置环境变量。

   - 确保所设置的 `gatewayip` 位于与 `ipaddr` 相同的子网内。例如，如果 `ipaddr` 是 `192.168.1.106`，子网掩码是 `255.255.255.0`，则 `gatewayip` 应设置为 `192.168.1.x` 范围内的地址。

   - 确保网络中没有其他设备使用相同的 IP 地址或 MAC 地址，以避免网络冲突。

```shell
setenv ipaddr 192.168.5.9        //开发板 IP 地址，这个需要和虚拟机处于同一网段
setenv ethaddr b8:ae:1d:01:00:00 //开发板网卡 MAC 地址
setenv gatewayip 192.168.5.1     //开发板默认网关
setenv netmask 255.255.255.0     //开发板子网掩码
setenv serverip 192.168.5.11     //服务器地址，也就是 Ubuntu 地址
saveenv                          //保存环境变量

setenv ipaddr 192.168.5.9    
setenv ethaddr b8:ae:1d:01:00:00 
setenv gatewayip 192.168.5.1  
setenv netmask 255.255.255.0    
setenv serverip 192.168.5.11  
saveenv                        
```


## 完善相关板级支持

### 修改 board 名字

1. 进入 `${sailing_uboot_4.1.15_2.1.0}/board/freescale/mx6ull_sailing_14x14_emmc/mx6ull_sailing_14x14_emmc.c` 进行如下修改。

```c
int checkboard(void)
{
	// if (is_mx6ull_9x9_evk())
	// 	puts("Board: MX6ULL 9x9 EVK\n");
	// else
	// 	puts("Board: MX6ULL 14x14 EVK\n");
	puts("Board: MX6ULL SAILING 14x14 EMMC\n");

	return 0;
}
```

2. 找到 `board_late_init()` 函数进行如下修改。

```c
int board_late_init(void)
{
    //...
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	setenv("board_name", "SAILING");
	setenv("board_rev", "14X14");
	// if (is_mx6ull_9x9_evk())
	// 	setenv("board_rev", "9X9");
	// else
	// 	setenv("board_rev", "14X14");
#endif
    //...
}
```

### 修改默认查找的 dtb 文件名

1. `${sailing_uboot_4.1.15_2.1.0}/include/configs/mx6ull_sailing_14x14_emmc.h` 进行如如下修改。

```h
// 修改前
#define CONFIG_BOOTCOMMAND \
	   "run findfdt;" \
	   "mmc dev ${mmcdev};" \
	   "mmc dev ${mmcdev}; if mmc rescan; then " \
		   "if run loadbootscript; then " \
			   "run bootscript; " \
		   "else " \
			   "if run loadimage; then " \
				   "run mmcboot; " \
			   "else run netboot; " \
			   "fi; " \
		   "fi; " \
	   "else run netboot; fi"
// 修改后
#define CONFIG_BOOTCOMMAND \
    "mmc dev 1;" \
    "fatload mmc 1:1 0x80800000 zImage;" \
    "fatload mmc 1:1 0x83000000 imx6ull-14x14-emmc-7-1024x600-c.dtb;" \
    "bootz 0x80800000 - 0x83000000;"

#define CONFIG_BOOTARGS \
    "console=ttymxc0,115200 root=/dev/mmcblk1p2 rootwait rw;"
```

### 与 menuconfig 适配

1. 因为在 `${sailing_uboot_4.1.15_2.1.0}/include/configs/mx6ull_sailing_14x14_emmc.h` 中默认设置，如果开启的 CMD_NET ，那么 CMD_PING 和 CMD_DHCP 也会默认开启，这将会导致我们打开 menuconfig 时候，命名 ping 和 dhcp 命令没有选择，但却开启了的情况。

```
#ifdef CONFIG_CMD_NET
#define CONFIG_CMD_PING
#define CONFIG_CMD_DHCP
```

2. 为了解决上述问题，我们先在 `${sailing_uboot_4.1.15_2.1.0}/include/configs/mx6ull_sailing_14x14_emmc.h` 中删除  CMD_PING 和 CMD_DHCP 。

```h
#ifdef CONFIG_CMD_NET
```

3. 修改 `${sailing_uboot_4.1.15_2.1.0}/cmd/Kconfig`  的反向选择。

```Kconfig
config CMD_NET
	bool "bootp, tftpboot"
        select NET
        select CMD_PING
        select CMD_DHCP
	default y
	help
	  Network commands.
	  bootp - boot image via network using BOOTP/TFTP protocol
	  tftpboot - boot image via network using TFTP protocol
```



# 问题

## 旧的环境变量导致的问题

1. 重新烧录 U-Boot 到开发板时，**新的 U-Boot 映像可能不会覆盖这些存储在非易失性存储器中的环境变量**。因此，**之前保存的环境变量仍然存在，并可能优先于您在新版本 U-Boot 中的默认配置**。这意味着，即使您在新的 U-Boot 中修改了相关参数，系统仍可能使用旧的环境变量值，从而导致修改未生效。
2. 在 U-Boot 命令行界面下，执行以下命令以恢复默认环境变量并保存：

```shell
env default -a
saveenv
```

## 关于uboot下出现data abort错误导致重启解决办法

1. 打开 `${uboot_PATH}/u-boot.map` 文件，找到 `start.o` 文件路径，找到对应的 `start.S` 文件，进行如下修改即可。

```S
/* 修改前*/
	/*
	 * disable MMU stuff and caches
	 */
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #0x00002000	@ clear bits 13 (--V-)
	bic	r0, r0, #0x00000007	@ clear bits 2:0 (-CAM)
	orr	r0, r0, #0x00000002	@ set bit 1 (--A-) Align
	orr	r0, r0, #0x00000800	@ set bit 11 (Z---) BTB
/* 修改后 */
    /*
     * disable MMU stuff and caches
     */
    mrc	p15, 0, r0, c1, c0, 0
    bic	r0, r0, #0x00002000	@ clear bits 13 (--V-)
    bic	r0, r0, #0x00000007	@ clear bits 2:0 (-CAM)
    orr	r0, r0, #0x00000000	@ set bit 1 (--A-) Align
    orr	r0, r0, #0x00000800	@ set bit 11 (Z---) BTB
```