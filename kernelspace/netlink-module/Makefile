# Add netlink to /etc/modules in order to load the module on boot
obj-m += netlink.o
netlink-objs := smv-netlink.o
PWD := $(PWD)
all:
	sudo make -C ../linux-4.4.5-smv/ M=$(PWD) CFLAGS="-g3" modules
	sudo cp ./netlink.ko /lib/modules/4.4.5/
clean:
	rm *.o *.ko *.mod.c *.symvers *.order

