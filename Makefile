obj-m := encryptor.o

KERNEL_DIR = /lib/modules/$(shell uname -r)/build

all:
		$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
		gcc -o encryptor_app encryptor_app.c
		
clean:
		$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
		rm encryptor_app
