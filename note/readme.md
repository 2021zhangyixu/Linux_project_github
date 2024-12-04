# 暂存的测试程序

1. 代码优化顺序：
   - 平台总线模型
   - 设备树
   - 尝试支持多个次设备号
   - 二次封装 API
2. 阻塞 IO 实验代码在适配好平台总线模型，设备树，中断之后再来实验。

```c
#define CONFIG_BLOCKING_IO 1

typedef struct chr_drv {
	int major;                 /*< Master device number */
	int minor;                 /*< Secondary device number */
	dev_t dev_num;             /*< Device number of type dev_t consisting of primary device number and secondary device number */
	struct cdev chr_cdev;      /*< Character device structure */
	struct class *chr_class;   /*< Device class */
    struct device *chr_device; /*< Device instance */
    char kbuf[KBUF_MAX_SIZE];  /*< Kernel buffer */
    struct gpio_desc *gpios;   /*< GPIO pin descriptor */
#ifdef CONFIG_BLOCKING_IO
    int block_flag;            /*< If it is blocking IO, this flag bit will be used */
#endif // CONFIG_BLOCKING_IO
}chr_drv;


#ifdef CONFIG_BLOCKING_IO
DECLARE_WAIT_QUEUE_HEAD(read_wq);
#endif // CONFIG_BLOCKING_IO
```

# 接下来要适配的内容

1. 中断
2. 四种文件 IO
3. 定时器