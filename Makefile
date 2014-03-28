CC ?= gcc
CFLAGS ?= -g -O2
all : cedarx-gui

cedarx-gui : cedarx-gui.c
	$(CC) -Wall $(CFLAGS) cedarx-gui.c -o cedarx-gui -pthread -lX11

clean :
	rm -f cedarx-gui
