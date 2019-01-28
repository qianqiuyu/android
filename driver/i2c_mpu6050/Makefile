KERN_DIR = /work/system/linux-4.8.17

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= mpu_drv.o
obj-m	+= mpu_dev.o
#arm-linux-gcc -o i2c_test i2c_test.c
