CC = cc
CFLAGS ?= -O2 -Wall -Wextra
LUA ?= lua5.4
LUA_INCLUDE ?= /usr/include/lua5.4
MODULE = src/luac/luaaoi.so
SOURCES = src/c/aoi_manager.c src/c/aoi.c

.PHONY: all test clean
all: $(MODULE)

$(MODULE): $(SOURCES) src/c/aoi.h $(LUA_INCLUDE)/lua.h $(LUA_INCLUDE)/luaconf.h $(LUA_INCLUDE)/lauxlib.h Makefile | src/luac
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -fPIC -shared -I"$(LUA_INCLUDE)" $(LDFLAGS) -o $@ $(SOURCES) $(LDLIBS)

src/luac:
	mkdir -p $@

src/luac/test_alloc: src/c/test_alloc.c src/c/aoi.c src/c/aoi.h Makefile | src/luac
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 $(LDFLAGS) -o $@ src/c/test_alloc.c src/c/aoi.c -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc $(LDLIBS)

test: $(MODULE) src/luac/test_alloc
	LUA_CPATH='./src/luac/?.so;;' $(LUA) src/lua/test.lua
	./src/luac/test_alloc

clean:
	$(RM) $(MODULE) src/luac/test_alloc
