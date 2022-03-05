all: respondd.so libolsrdhelper.so olsrd.so

CFLAGS += -Wall -D_GNU_SOURCE

ifeq ($(origin PKG_CONFIG), undefined)
  PKG_CONFIG = pkg-config
  ifeq ($(shell which $(PKG_CONFIG) 2>/dev/null),)
    $(error $(PKG_CONFIG) not found)
  endif
endif

CFLAGS += $(shell pkg-config --cflags json-c)
LDFLAGS += $(shell pkg-config --libs json-c)

SOURCES_HELPER = libolsrdhelper.c uclient.c
FILES_HELPER = $(SOURCES_HELPER) libolsrdhelper.h uclient.h

SOURCES_RESPONDD = respondd.c
FILES_RESPONDD = $(SOURCES) respondd-common.h

SOURCES_LUA = olsrd.c

respondd.so: $(FILES_RESPONDD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -shared -fPIC -fvisibility=hidden -D_GNU_SOURCE -o $@ $(SOURCES_RESPONDD) $(LDLIBS) -lgluonutil

libolsrdhelper.so:
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC -fvisibility=hidden -D_GNU_SOURCE -o $@ $(SOURCES_HELPER) $(LDLIBS) -lgluonutil

libolsrdhelper.so:
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fPIC -fvisibility=hidden -D_GNU_SOURCE -o $@ $(SOURCES_HELPER) $(LDLIBS) -lgluonutil

olsrd.so: $(SOURCES_LUA)
	$(CC) $(LUA_CFLAGS) $(CFLAGS) $(LDFLAGS) -shared -fPIC -D_GNU_SOURCE -o $@ $(SOURCES_LUA) $(LDLIBS) -lgluonutil
