KERN_DIR = /work/system/linux-4.8.17

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= rc522_dev.o
obj-m	+= rc522_drv.o
obj-m   += 24L01.o
#arm-linux-gcc -o i2c_test i2c_test.c
