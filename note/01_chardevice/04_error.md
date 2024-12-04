# 错误处理

## 宏介绍

1. 检查给定的指针是否是一个表示错误的指针。

```c
/* 作用 : 检查指针是否表示一个错误
 * ptr : 要被检查的指针
 * 返回 : 如果 ptr 是错误指针，返回 true ，即返回 1。否则返回 false，及 0
 */
IS_ERR(ptr);

/******* 示例 *******/
s_char_drv.chr_class = class_create(THIS_MODULE,"chr_class");
if (IS_ERR(s_char_drv.chr_class)) {
    printk("class_create is error\n");
    ret = PTR_ERR(s_char_drv.chr_class);
    goto err_class_create;
}
```

2. 从一个错误指针中提取出表示错误码的整数值，这个宏通常与 `ERR_PTR()` 配合使用。

```c
/* 作用 : 从一个错误指针中提取出表示错误码的整数值
 * ptr : 要被提取错误码的错误指针
 * 返回 : 返回一个与 ptr 对应的错误码
 */
PTR_ERR(ptr);

/******* 示例 *******/
s_char_drv.chr_class = class_create(THIS_MODULE,"chr_class");
if (IS_ERR(s_char_drv.chr_class)) {
    printk("class_create is error\n");
    ret = PTR_ERR(s_char_drv.chr_class);
    goto err_class_create;
}
```

3. 将一个错误码（通常是负值）转换为一个指针，这个指针指向一个表示错误的特殊地址，在 Linux 内核中，这个地址通常是负值的。

```c
/* 作用 : 将错误码封装为一个指针，这样调用者可以通过检查返回的指针是否为错误指针来判断操作是否成功
 * ptr : 要被封装为指针的错误码
 * 返回 : 返回错误指针
 */
ERR_PTR(err);

/******* 示例 *******/
void *some_function(void) {
    if (something_went_wrong) {
        return ERR_PTR(-ENOMEM);  // 返回表示内存不足的错误指针
    }
    return some_valid_pointer;
}
```



## 综合示例

```c
static int __init chr_drv_init(void)
{
	int ret;
	/*< 1. Request equipment number */
	if (major) { /*< Static Device ID Setting */
    	s_char_drv.dev_num = MKDEV(major,minor);
    	printk("major is %d\n",major);
    	printk("minor is %d\n",minor);
    	ret = register_chrdev_region(s_char_drv.dev_num,1,"chrdev_name");
        if (ret < 0) {
            printk("register_chrdev_region is error\n");
            goto err_chrdev;
        }
        printk("register_chrdev_region is ok\n");
    } else { /*< Dynamic Device ID Setting */
        ret = alloc_chrdev_region(&s_char_drv.dev_num,0,1,"chrdev_num");
        if (ret < 0) {
            printk("alloc_chrdev_region is error\n");
            goto err_chrdev;
        }
        major=MAJOR(s_char_drv.dev_num);
        minor=MINOR(s_char_drv.dev_num);
        printk("major is %d\n",major);
        printk("minor is %d\n",minor);
    }
	/*< 2. Registered character device */
    cdev_init(&s_char_drv.chr_cdev,&cdev_test_ops);
	s_char_drv.chr_cdev.owner = THIS_MODULE;
    ret = cdev_add(&s_char_drv.chr_cdev,s_char_drv.dev_num,1);
    if(ret < 0 ){
        printk("cdev_add is error\n");
        goto  err_chr_add;
    }
    printk("cdev_add is ok\n");
	/*< 3. Creating a Device Node */
    s_char_drv.chr_class = class_create(THIS_MODULE,"chr_class");
    if (IS_ERR(s_char_drv.chr_class)) {
        printk("class_create is error\n");
        ret = PTR_ERR(s_char_drv.chr_class);
        goto err_class_create;
    }
    s_char_drv.chr_device = device_create(s_char_drv.chr_class,NULL,s_char_drv.dev_num,NULL,"chr_device");
    if (IS_ERR(s_char_drv.chr_device)) {
        printk("device_create is error\n");
        ret = PTR_ERR(s_char_drv.chr_device);
        goto err_device_create;
    }
    return 0;

err_device_create:
    class_destroy(s_char_drv.chr_class);
err_class_create:
    cdev_del(&s_char_drv.chr_cdev);
err_chr_add:
    unregister_chrdev_region(s_char_drv.dev_num, 1);
err_chrdev:
    return ret;
}
```

# 