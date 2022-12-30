all: respondd.so libolsrdhelper.so olsrd.so olsrd-debug olsr-respondd

CFLAGS += -Wall -D_GNU_SOURCE -g

ifeq ($(origin PKG_CONFIG), undefined)
  PKG_CONFIG = pkg-config
  ifeq ($(shell which $(PKG_CONFIG) 2>/dev/null),)
    $(error $(PKG_CONFIG) not found)
  endif
endif

CFLAGS += $(shell pkg-config --cflags json-c)
LDFLAGS += $(shell pkg-config --libs json-c)

SOURCES_HELPER = libolsrdhelper.c libolsrdhelper-neigh.c uclient.c arp.c
FILES_HELPER = $(SOURCES_HELPER) libolsrdhelper.h uclient.h arp.h

SOURCES_RESPONDD = respondd.c respondd-nodeinfo.c respondd-neighbours.c
FILES_RESPONDD = $(SOURCES_RESPONDD) respondd-common.h

SOURCES_DEBUG = olsrd-debug.c $(SOURCES_RESPONDD)
FILES_DEBUG = $(SOURCES_DEBUG)

SOURCES_OLSR_RESPONDD = olsr-respondd.c $(SOURCES_RESPONDD)
FILES_OLSR_RESPONDD = $(SOURCES_OLSR_RESPONDD)

SOURCES_LUA = olsrd.c
FILES_LUA = $(SOURCES_LUA)

respondd.so: libolsrdhelper.so $(FILES_RESPONDD)
	$(CC) $(CFLAGS) $(LDFLAGS) -I. -L. -shared -fPIC -fvisibility=hidden -o $@ $(SOURCES_RESPONDD) $(LDLIBS) -lgluonutil -lolsrdhelper -luci

olsr-respondd: respondd.so $(FILES_OLSR_RESPONDD)
	$(CC) $(LUA_CFLAGS) $(CFLAGS) $(LDFLAGS) -fPIE -I. -L. -o $@ $(SOURCES_OLSR_RESPONDD) $(LDLIBS) -lgluonutil -lolsrdhelper -luci

libolsrdhelper.so: libolsrdhelper.h libolsrdhelper.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC -o $@ $(SOURCES_HELPER) $(LDLIBS) -lgluonutil -luclient

olsrd.so: libolsrdhelper.so $(FILES_LUA)
	$(CC) $(LUA_CFLAGS) $(CFLAGS) $(LDFLAGS) -I. -L. -shared -fPIC -o $@ $(SOURCES_LUA) $(LDLIBS) -lgluonutil -lolsrdhelper -luci -llua-jsonc

olsrd-debug: libolsrdhelper.so $(FILES_DEBUG)
	$(CC) $(LUA_CFLAGS) $(CFLAGS) $(LDFLAGS) -fPIE -I. -L. -o $@ $(SOURCES_DEBUG) $(LDLIBS) -lgluonutil -lolsrdhelper -luci -llua-jsonc -llua
