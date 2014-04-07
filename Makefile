CC ?= gcc
CFLAGS ?= -O2
FIFO_PATH ?= /tmp/cedarxplayertest
PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib/cedarxplayertest
CPT_PATH ?= .
CPT_BIN ?= CedarXPlayerTest-1.4.1
BINLIBS = $(CPT_PATH)/libiconv.so.2
BINLIBS += $(CPT_PATH)/libsalsa.so.0
BINLIBS += $(CPT_PATH)/libcharset.so.1
LD_LINUX ?= /lib/ld-linux-armhf.so.3
all : cedarx-gui preload.so

cedarx-gui : cedarx-gui.c
	$(CC) -Wall $(CFLAGS) -g cedarx-gui.c -o cedarx-gui -pthread -lX11 -DCPT_PATH=\"$(LIBDIR)/\" -DCPT_BIN=\"$(CPT_BIN)\" -DLD_LINUX=\"$(LD_LINUX)\"
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
uninstall : $(PREFIX)/bin/cedarx-gui $(LIBDIR)/libcharset.so.1 $(LIBDIR)/libsalsa.so.0 $(LIBDIR)/libiconv.so.2 $(LIBDIR)/$(CPT_BIN) $(LIBDIR)/preload.so
	rm -f $(PREFIX)/bin/cedarx-gui $(LIBDIR)/libcharset.so.1 $(LIBDIR)/libsalsa.so.0 $(LIBDIR)/libiconv.so.2 $(LIBDIR)/$(CPT_BIN) $(LIBDIR)/preload.so
	-rm -d $(LIBDIR) $(PREFIX)/bin $(PREFIX)/lib $(PREFIX)
