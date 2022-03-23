all: neighbours-olsrd

CFLAGS += -Wall -D_GNU_SOURCE

ifeq ($(origin PKG_CONFIG), undefined)
  PKG_CONFIG = pkg-config
  ifeq ($(shell which $(PKG_CONFIG) 2>/dev/null),)
    $(error $(PKG_CONFIG) not found)
  endif
endif

CFLAGS_JSONC = $(shell pkg-config --cflags json-c)
LDFLAGS_JSONC = $(shell pkg-config --libs json-c)


neighbours-olsrd: neighbours-olsrd.c
	$(CC) $(CFLAGS) $(CFLAGS_JSONC) $(LDFLAGS) $(LDFLAGS_JSONC) -o $@ $^ $(LDLIBS)
