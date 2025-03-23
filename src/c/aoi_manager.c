#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>
#include "aoi.h"

#define AOI_METATABLE "aoi.manager"

typedef struct {
    struct aoi_space *space;
    lua_State *L;
    int callback_ref;
    struct {
        uint32_t *watchers;
        uint32_t *markers;
        size_t count;
        size_t max_size;
    } pairs;
} aoi_manager_t;

static void aoi_callback(void *ud, uint32_t watcher, uint32_t marker) {
    aoi_manager_t *manager = (aoi_manager_t *)ud;
    
    if (manager->pairs.count >= manager->pairs.max_size) {
        memmove(manager->pairs.watchers, manager->pairs.watchers + 1, 
                (manager->pairs.max_size - 1) * sizeof(uint32_t));
        memmove(manager->pairs.markers, manager->pairs.markers + 1, 
                (manager->pairs.max_size - 1) * sizeof(uint32_t));
        manager->pairs.count = manager->pairs.max_size - 1;
    }
    
    manager->pairs.watchers[manager->pairs.count] = watcher;
    manager->pairs.markers[manager->pairs.count] = marker;
    manager->pairs.count++;
}

static int lua_aoi_create(lua_State *L) {
    size_t max_size = luaL_optinteger(L, 1, 1024);
    
    aoi_manager_t *manager = lua_newuserdata(L, sizeof(aoi_manager_t));
    manager->space = aoi_new();
    manager->L = L;
    manager->callback_ref = LUA_REFNIL;
    
    manager->pairs.watchers = malloc(max_size * sizeof(uint32_t));
    manager->pairs.markers = malloc(max_size * sizeof(uint32_t));
    manager->pairs.count = 0;
    manager->pairs.max_size = max_size;
    
    if (!manager->pairs.watchers || !manager->pairs.markers) {
        free(manager->pairs.watchers);
        free(manager->pairs.markers);
        return luaL_error(L, "内存分配失败");
    }
    
    luaL_getmetatable(L, AOI_METATABLE);
    lua_setmetatable(L, -2);
    
    return 1;
}

static int lua_aoi_register(lua_State *L) {
    aoi_manager_t *manager = luaL_checkudata(L, 1, AOI_METATABLE);
    
    if (manager->callback_ref != LUA_REFNIL) {
        luaL_unref(L, LUA_REGISTRYINDEX, manager->callback_ref);
    }
    
    lua_pushvalue(L, 2);
    manager->callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

static int lua_aoi_remove(lua_State *L) {
    aoi_manager_t *manager = luaL_checkudata(L, 1, AOI_METATABLE);
    uint32_t id = luaL_checkinteger(L, 2);
    
    float pos[3] = {0, 0, 0};
    aoi_update(manager->space, id, "d", pos);
    return 0;
}

static int lua_aoi_update(lua_State *L) {
    aoi_manager_t *manager = luaL_checkudata(L, 1, AOI_METATABLE);
    uint32_t id = luaL_checkinteger(L, 2);
    float x = luaL_checknumber(L, 3);
    float y = luaL_checknumber(L, 4);
    const char *mode = luaL_checkstring(L, 5);
    
    float pos[3] = {x, y, 0};
    aoi_update(manager->space, id, mode, pos);
    return 0;
}

static int lua_aoi_gc(lua_State *L) {
    aoi_manager_t *manager = luaL_checkudata(L, 1, AOI_METATABLE);
    
    if (manager->callback_ref != LUA_REFNIL) {
        luaL_unref(L, LUA_REGISTRYINDEX, manager->callback_ref);
    }
    
    free(manager->pairs.watchers);
    free(manager->pairs.markers);
    
    aoi_release(manager->space);
    return 0;
}

static int lua_aoi_message(lua_State *L) {
    aoi_manager_t *manager = luaL_checkudata(L, 1, AOI_METATABLE);
    
    manager->pairs.count = 0;
    
    aoi_message(manager->space, aoi_callback, manager);
    
    if (manager->callback_ref != LUA_REFNIL) {
        lua_rawgeti(manager->L, LUA_REGISTRYINDEX, manager->callback_ref);
        
        lua_createtable(L, manager->pairs.count, 0);
        
        for (size_t i = 0; i < manager->pairs.count; i++) {
            lua_createtable(L, 2, 0);
            
            lua_pushnumber(L, manager->pairs.watchers[i]);
            lua_rawseti(L, -2, 1);
            
            lua_pushnumber(L, manager->pairs.markers[i]);
            lua_rawseti(L, -2, 2);
            
            lua_rawseti(L, -2, i + 1);
        }
        
        if (lua_pcall(manager->L, 1, 0, 0) != 0) {
            fprintf(stderr, "Error in AOI callback: %s\n", lua_tostring(manager->L, -1));
            lua_pop(manager->L, 1);
        }
    }
    
    return 0;
}

static const struct luaL_Reg aoi_methods[] = {
    {"create", lua_aoi_create},
    {"register", lua_aoi_register},
    {"remove", lua_aoi_remove},
    {"update", lua_aoi_update},
    {"message", lua_aoi_message},
    {NULL, NULL}
};

int luaopen_luaaoi(lua_State *L) {
    luaL_newmetatable(L, AOI_METATABLE);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, aoi_methods, 0);
    lua_pushcfunction(L, lua_aoi_gc);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
    
    luaL_newlib(L, aoi_methods);
    return 1;
}