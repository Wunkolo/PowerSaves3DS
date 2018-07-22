obj-m += powersaves.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

install:
	-rmmod  powersaves
	insmod powersaves.ko

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
