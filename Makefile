CC ?= gcc
CFLAGS ?= -g -O2
all : cedarx-gui preload.so

cedarx-gui : cedarx-gui.c
	$(CC) -Wall $(CFLAGS) cedarx-gui.c -o cedarx-gui -pthread -lX11

preload.so : preload.c
	$(CC) -Wall $(CFLAGS) preload.c -fPIC -shared -ldl -o preload.so
clean :
	rm -f cedarx-gui
