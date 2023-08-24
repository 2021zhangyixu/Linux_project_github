
KERN_DIR = /home/book/100ask_imx6ull-sdk/Linux-4.9.88


all:
	make -C $(KERN_DIR) M=`pwd` modules 
	$(CROSS_COMPILE)gcc -o waitqueue_read waitqueue_read.c 
	$(CROSS_COMPILE)gcc -o waitqueue_write waitqueue_write.c 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order
	rm -f waitqueue_read waitqueue_write

obj-m	+= waitqueue_drv.o  key_driver.o