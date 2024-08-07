# Define the object files to be built
obj-m := message_slot.o

# KDIR is the kernel directory, where the kernel's build files are located
KDIR := /lib/modules/$(shell uname -r)/build

# PWD is the current working directory
PWD := $(shell pwd)

# The 'all' target builds the kernel module
all:
	# Use the kernel build system to compile the module
	$(MAKE) -C $(KDIR) M=$(PWD) modules
 
# The 'clean' target cleans up the build environment
clean:
	# Use the kernel build system to clean up files generated during compilation
	$(MAKE) -C $(KDIR) M=$(PWD) clean
