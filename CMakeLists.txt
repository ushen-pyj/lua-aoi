cmake_minimum_required(VERSION 3.10)
project(lua-aoi C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 添加 Lua 库的构建
file(GLOB LUA_SOURCES 
    "${CMAKE_SOURCE_DIR}/3rd/lua/*.c"
)

add_library(lua STATIC ${LUA_SOURCES})

# 添加 -fPIC 标志
set_property(TARGET lua PROPERTY POSITION_INDEPENDENT_CODE ON)

# 设置 lua 静态库的输出路径
set_target_properties(lua PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/src/luac"
)

target_include_directories(lua PUBLIC
    ${CMAKE_SOURCE_DIR}/3rd/lua
)

# 添加 Lua 需要的系统库
target_link_libraries(lua PUBLIC
    m   # math library
    dl  # dynamic linking
)

# 设置 Lua 路径
set(LUA_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/3rd/lua)
set(LUA_LIBRARIES lua)

# 添加 AOI 子模块
add_subdirectory(aoi)

# 添加源文件
set(SOURCES
    src/c/aoi_manager.c
)

# 创建共享库
add_library(luaaoi SHARED ${SOURCES})

# 设置输出名称和路径
set_target_properties(luaaoi PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/src/luac"
)

# 包含目录
target_include_directories(luaaoi PRIVATE
    ${LUA_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/aoi
)

# 链接库
target_link_libraries(luaaoi PRIVATE
    ${LUA_LIBRARIES}
    aoi
)