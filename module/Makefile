obj-m += cuckoo_hash_kernel.o

PWD := ~/Documents/GitHub/In-kernel-lock-less-Cuckoo-Hashing/module

all: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

 

clean: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean