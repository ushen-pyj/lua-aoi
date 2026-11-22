#include <float.h>
#include <limits.h>
#include <stdint.h>

#include <lua.h>
#include <lauxlib.h>
#include "aoi.h"

#define AOI_METATABLE "aoi.manager"
#define BATCH_METATABLE "aoi.batch"

struct pair {
	uint32_t watcher;
	uint32_t marker;
};

struct manager {
	struct aoi_space *space;
	size_t capacity;
};

struct batch {
	size_t capacity;
	size_t next;
	size_t count;
	struct pair pairs[];
};

static struct manager *
check_manager(lua_State *L) {
	struct manager *m = luaL_checkudata(L, 1, AOI_METATABLE);
	luaL_argcheck(L, m->space != NULL, 1, "manager is closed");
	return m;
}

static uint32_t
check_id(lua_State *L) {
	lua_Integer id = luaL_checkinteger(L, 2);
	/* Reserve UINT32_MAX for compatibility with the original AOI map. */
	luaL_argcheck(L, id >= 0 && (lua_Unsigned)id < UINT32_MAX, 2, "invalid object id");
	return (uint32_t)id;
}

static float
check_coordinate(lua_State *L, int index) {
	lua_Number n = luaL_checknumber(L, index);
	luaL_argcheck(L, n >= -FLT_MAX && n <= FLT_MAX, index, "invalid coordinate");
	return (float)n;
}

static int
lcreate(lua_State *L) {
	lua_Integer cap = luaL_optinteger(L, 1, 1024);
	luaL_argcheck(L, cap > 0 && cap <= INT_MAX && cap <= LUA_MAXINTEGER / 2 &&
		(lua_Unsigned)cap <= (SIZE_MAX - sizeof(struct batch)) / sizeof(struct pair),
		1, "invalid event capacity");
	struct manager *m = lua_newuserdatauv(L, sizeof(*m), 1);
	m->space = NULL;
	m->capacity = (size_t)cap;
	luaL_setmetatable(L, AOI_METATABLE);
	m->space = aoi_new();
	if (!m->space)
		return luaL_error(L, "out of memory");
	return 1;
}

static int
lremove(lua_State *L) {
	struct manager *m = check_manager(L);
	uint32_t id = check_id(L);
	float pos[3] = {0};
	aoi_update(m->space, id, "d", pos);
	return 0;
}

static int
lupdate(lua_State *L) {
	struct manager *m = check_manager(L);
	uint32_t id = check_id(L);
	float pos[3] = { check_coordinate(L, 3), check_coordinate(L, 4), 0 };
	size_t len;
	luaL_checktype(L, 5, LUA_TSTRING);
	const char *mode = luaL_checklstring(L, 5, &len);
	for (size_t i = 0; i < len; ++i) {
		luaL_argcheck(L, mode[i] == 'm' || mode[i] == 'w' || mode[i] == 'd',
			5, "invalid AOI mode");
	}
	aoi_update(m->space, id, mode, pos);
	return 0;
}

static int
lclose(lua_State *L) {
	struct manager *m = luaL_checkudata(L, 1, AOI_METATABLE);
	if (m->space) {
		aoi_release(m->space);
		m->space = NULL;
		lua_pushnil(L);
		lua_setiuservalue(L, 1, 1);
	}
	return 0;
}

static void
collect_pair(void *ud, uint32_t watcher, uint32_t marker) {
	struct batch *m = ud;
	m->pairs[m->next] = (struct pair){ watcher, marker };
	if (++m->next == m->capacity)
		m->next = 0;
	if (m->count < m->capacity)
		++m->count;
}

static struct batch *
read_batch(lua_State *L, struct manager *m) {
	/* Anchor this call's buffer on its stack. A GC finalizer may call message
	 * again while Lua grows the output table. */
	int buffer = lua_gettop(L) + 1;
	struct batch *batch;
	size_t bytes = sizeof(*batch) + m->capacity * sizeof(struct pair);
	lua_getiuservalue(L, 1, 1);
	batch = luaL_testudata(L, buffer, BATCH_METATABLE);
	if (!batch || lua_rawlen(L, buffer) != bytes) {
		lua_pop(L, 1);
		batch = lua_newuserdatauv(L, bytes, 0);
		luaL_setmetatable(L, BATCH_METATABLE);
		check_manager(L);
	} else {
		lua_pushnil(L);
		lua_setiuservalue(L, 1, 1);
	}
	batch->capacity = m->capacity;
	batch->count = batch->next = 0;
	aoi_message(m->space, collect_pair, batch);
	return batch;
}

static void
return_batch(lua_State *L, struct manager *m, int buffer) {
	if (m->space) {
		lua_pushvalue(L, buffer);
		lua_setiuservalue(L, 1, 1);
	}
	lua_remove(L, buffer);
}

/* message(out) -> pair count. Reuse the caller's sequence across ticks. */
static int
lmessage(lua_State *L) {
	struct manager *m = check_manager(L);
	luaL_checktype(L, 2, LUA_TTABLE);
	size_t oldlen = lua_rawlen(L, 2);
	struct batch *batch = read_batch(L, m);
	int buffer = lua_gettop(L);
	size_t count = batch->count;
	size_t index = count == batch->capacity ? batch->next : 0;
	for (size_t i = 0; i < count; ++i) {
		struct pair p = batch->pairs[index];
		lua_pushinteger(L, p.watcher);
		lua_rawseti(L, 2, (lua_Integer)i * 2 + 1);
		lua_pushinteger(L, p.marker);
		lua_rawseti(L, 2, (lua_Integer)i * 2 + 2);
		if (++index == batch->capacity)
			index = 0;
	}
	for (size_t i = oldlen; i > 2 * count; --i) {
		lua_pushnil(L);
		lua_rawseti(L, 2, (lua_Integer)i);
	}
	return_batch(L, m, buffer);
	lua_pushinteger(L, (lua_Integer)count);
	return 1;
}

static const luaL_Reg methods[] = {
	{ "create", lcreate },
	{ "remove", lremove },
	{ "update", lupdate },
	{ "message", lmessage },
	{ "close", lclose },
	{ NULL, NULL },
};

int
luaopen_luaaoi(lua_State *L) {
	luaL_checkversion(L);
	luaL_newmetatable(L, BATCH_METATABLE);
	lua_pop(L, 1);
	luaL_newlib(L, methods);
	luaL_newmetatable(L, AOI_METATABLE);
	lua_pushcfunction(L, lclose);
	lua_setfield(L, -2, "__gc");
	lua_pushvalue(L, -2);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
	return 1;
}
