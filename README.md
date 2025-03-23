# Lua AOI 项目

这是一个基于 cloudwu/aoi 的 Lua 绑定项目，用于实现游戏中的 AOI (Area of Interest) 系统。

## 项目结构

```
.
├── aoi/              # AOI 子模块
├── src/              # 源代码目录
│   ├── c/           # C 代码
│   │   └── aoi_manager.c  # AOI 管理器
│   └── lua/         # Lua 代码
│       ├── aoi.lua      # AOI 对象管理
│       ├── player.lua   # 玩家对象
│       └── server.lua   # WebSocket 服务器
├── CMakeLists.txt   # CMake 构建文件
└── README.md        # 项目说明文档
```

## 功能特性

- AOI 系统集成
- Lua 回调函数支持
- 玩家对象管理
- WebSocket 服务器
- 实时位置同步

## 构建要求

- CMake 3.10+
- Lua 5.4+
- libuv (用于 WebSocket 支持)