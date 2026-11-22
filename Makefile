CC = cc
CFLAGS ?= -O2 -Wall -Wextra
LUA ?= lua5.4
LUA_INCLUDE ?= /usr/include/lua5.4
MODULE = src/luac/luaaoi.so
SOURCES = src/c/aoi_manager.c aoi/aoi.c

.PHONY: all test clean
all: $(MODULE)

$(MODULE): $(SOURCES) aoi/aoi.h $(LUA_INCLUDE)/lua.h $(LUA_INCLUDE)/luaconf.h $(LUA_INCLUDE)/lauxlib.h Makefile | src/luac
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -fPIC -shared -Iaoi -I"$(LUA_INCLUDE)" $(LDFLAGS) -o $@ $(SOURCES) $(LDLIBS)

src/luac:
	mkdir -p $@

test: $(MODULE)
	LUA_CPATH='./src/luac/?.so;;' $(LUA) src/lua/test.lua

clean:
	$(RM) $(MODULE)
