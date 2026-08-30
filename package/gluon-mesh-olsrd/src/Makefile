CFLAGS += -Wall -D_GNU_SOURCE -g
CFLAGS += -Irespondd

PKG_CONFIG ?= pkg-config

CFLAGS += $(shell $(PKG_CONFIG) --cflags json-c libolsrdhelper)
LDLIBS += $(shell $(PKG_CONFIG) --libs json-c libolsrdhelper)

# respondd providers, shared by plugin and tools
SOURCES_PROVIDERS = respondd/neighbours.c respondd/nodeinfo.c respondd/statistics.c
FILES_PROVIDERS = $(SOURCES_PROVIDERS) respondd/providers.h

SOURCES_RESPONDD = respondd/respondd.c $(SOURCES_PROVIDERS)
FILES_RESPONDD = $(SOURCES_RESPONDD) $(FILES_PROVIDERS)

SOURCES_OLSR_RESPONDD = tools/olsr-respondd.c $(SOURCES_PROVIDERS)
FILES_OLSR_RESPONDD = $(SOURCES_OLSR_RESPONDD) $(FILES_PROVIDERS)

SOURCES_DEBUG = tools/olsrd-debug.c $(SOURCES_PROVIDERS)
FILES_DEBUG = $(SOURCES_DEBUG) $(FILES_PROVIDERS)

all: olsrd-debug olsr-respondd respondd.so

respondd.so: $(FILES_RESPONDD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -shared -fPIC -fvisibility=hidden -o $@ $(SOURCES_RESPONDD) $(LDLIBS) -lgluonutil -lgluon-neighbours

olsr-respondd: $(FILES_OLSR_RESPONDD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -fPIE -o $@ $(SOURCES_OLSR_RESPONDD) $(LDLIBS) -lgluonutil -lgluon-neighbours

olsrd-debug: $(FILES_DEBUG)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -fPIE -o $@ $(SOURCES_DEBUG) $(LDLIBS) -lgluonutil -lgluon-neighbours

clean:
	rm -f *.so olsrd-debug olsr-respondd
