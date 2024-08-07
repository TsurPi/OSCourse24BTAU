obj-m += message_slot.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -o message_sender message_sender.c
	gcc -o message_reader message_reader.c

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f message_sender message_reader
