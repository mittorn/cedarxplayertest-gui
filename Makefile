CC ?= gcc
CFLAGS ?= -g -O2
FIFO_PATH ?= /tmp/cedarxplayertest
PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib/cedarxplayertest
CPT_PATH ?= .
CPT_BIN ?= CedarXPlayerTest-1.4.1
BINLIBS = $(CPT_PATH)/libiconv.so.2
BINLIBS += $(CPT_PATH)/libsalsa.so.0
BINLIBS += $(CPT_PATH)/libcharset.so.1
all : cedarx-gui preload.so

cedarx-gui : cedarx-gui.c
	$(CC) -Wall $(CFLAGS) cedarx-gui.c -o cedarx-gui -pthread -lX11 -DCEDARXPLAYERTEST_COMMAND=\"/lib/ld-linux-armhf.so.3\ --library-path\ $(LIBDIR)\ $(LIBDIR)/CedarXPlayerTest-1.4.1\" -DFIFO_PATH=\"$(FIFO_PATH)\"  -DPRELOAD_PATH=\"$(LIBDIR)/preload.so\"

preload.so : preload.c
	$(CC) -Wall $(CFLAGS) preload.c -fPIC -shared -ldl -o preload.so
clean :
	rm -f cedarx-gui
	rm -f preload.so
install : cedarx-gui
	@mkdir -p $(PREFIX)/bin
	install -m 0755 cedarx-gui $(PREFIX)/bin
	@mkdir -p $(LIBDIR)
	install -m 0644 $(BINLIBS) $(CPT_PATH)/$(CPT_BIN) preload.so $(LIBDIR)
uninstall : $(PREFIX)/bin/cedarx-gui $(LIBDIR)/libcharset.so.1 $(LIBDIR)/libsalsa.so.0 $(LIBDIR)/libiconv.so.2 $(LIBDIR)/$(CPT_BIN)
	rm -f $(PREFIX)/bin/cedarx-gui $(LIBDIR)/libcharset.so.1 $(LIBDIR)/libsalsa.so.0 $(LIBDIR)/libiconv.so.2 $(LIBDIR)/$(CPT_BIN)
