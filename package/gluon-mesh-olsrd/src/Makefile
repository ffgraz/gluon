CFLAGS += -Wall -D_GNU_SOURCE -g
CFLAGS += -Ilib -Imacd -Irespondd

ifeq ($(origin PKG_CONFIG), undefined)
  PKG_CONFIG = pkg-config
  ifeq ($(shell which $(PKG_CONFIG) 2>/dev/null),)
    $(error $(PKG_CONFIG) not found)
  endif
endif

CFLAGS += $(shell pkg-config --cflags json-c)
LDFLAGS += $(shell pkg-config --libs json-c)

# libolsrdhelper, talks to olsrd
SOURCES_HELPER = lib/libolsrdhelper.c lib/neigh.c lib/uclient.c
FILES_HELPER = $(SOURCES_HELPER) lib/libolsrdhelper.h lib/uclient.h macd/olsr-macd.h

# respondd providers, shared by plugin and tools
SOURCES_PROVIDERS = respondd/neighbours.c respondd/nodeinfo.c respondd/statistics.c
FILES_PROVIDERS = $(SOURCES_PROVIDERS) respondd/providers.h

SOURCES_RESPONDD = respondd/respondd.c $(SOURCES_PROVIDERS)
FILES_RESPONDD = $(SOURCES_RESPONDD) $(FILES_PROVIDERS)

SOURCES_OLSR_RESPONDD = tools/olsr-respondd.c $(SOURCES_PROVIDERS)
FILES_OLSR_RESPONDD = $(SOURCES_OLSR_RESPONDD) $(FILES_PROVIDERS)

SOURCES_DEBUG = tools/olsrd-debug.c $(SOURCES_PROVIDERS)
FILES_DEBUG = $(SOURCES_DEBUG) $(FILES_PROVIDERS)

SOURCES_LUA = lua/olsrd.c
FILES_LUA = $(SOURCES_LUA)

SOURCES_MACD = macd/olsr-macd.c
FILES_MACD = $(SOURCES_MACD) macd/olsr-macd.h

all: olsrd-debug olsrd.so libolsrdhelper.so olsr-respondd olsr-macd respondd.so

respondd.so: libolsrdhelper.so $(FILES_RESPONDD)
	$(CC) $(CFLAGS) $(LDFLAGS) -L. -shared -fPIC -fvisibility=hidden -o $@ $(SOURCES_RESPONDD) $(LDLIBS) -lgluonutil -lolsrdhelper

olsr-respondd: libolsrdhelper.so $(FILES_OLSR_RESPONDD)
	$(CC) $(CFLAGS) $(LDFLAGS) -fPIE -L. -o $@ $(SOURCES_OLSR_RESPONDD) $(LDLIBS) -lgluonutil -lolsrdhelper

olsr-macd: $(FILES_MACD)
	$(CC) $(CFLAGS) $(LDFLAGS) -fPIE -o $@ $(SOURCES_MACD) $(LDLIBS) -lubox

libolsrdhelper.so: $(FILES_HELPER)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC -o $@ $(SOURCES_HELPER) $(LDLIBS) -lgluonutil -luclient

olsrd.so: libolsrdhelper.so $(FILES_LUA)
	$(CC) $(LUA_CFLAGS) $(CFLAGS) $(LDFLAGS) -L. -shared -fPIC -o $@ $(SOURCES_LUA) $(LDLIBS) -lgluonutil -lolsrdhelper -llua-jsonc

olsrd-debug: libolsrdhelper.so $(FILES_DEBUG)
	$(CC) $(CFLAGS) $(LDFLAGS) -fPIE -L. -o $@ $(SOURCES_DEBUG) $(LDLIBS) -lgluonutil -lolsrdhelper

clean:
	rm -f *.so *.o olsrd-debug olsr-respondd olsr-macd
