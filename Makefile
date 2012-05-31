obj-m := rapoov7_hotfix.o
rapoov7_hotfix-objs := main.o mod_base.o probe.o keystroke.o leds.o

KVERSION = $(shell uname -r)

all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
