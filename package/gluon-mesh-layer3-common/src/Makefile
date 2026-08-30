CFLAGS += -Wall -D_GNU_SOURCE -g -fPIC

ifeq ($(origin PKG_CONFIG), undefined)
  PKG_CONFIG = pkg-config
  ifeq ($(shell which $(PKG_CONFIG) 2>/dev/null),)
    $(error $(PKG_CONFIG) not found)
  endif
endif

CFLAGS += $(shell $(PKG_CONFIG) --cflags json-c)
LDFLAGS += $(shell $(PKG_CONFIG) --libs json-c)

all: libgluon-neighbours.so

libgluon-neighbours.so: gluon-neighbours.c gluon-neighbours.h
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o $@ gluon-neighbours.c $(LDLIBS) -lgluonutil

clean:
	rm -f *.so *.o
