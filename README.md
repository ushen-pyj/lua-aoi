# lua-aoi

Lua 5.4 的二维 AOI 模块。观察者（w）和标记者（m）的距离小于 30 时，可以收到一条包含双方 ID 的事件。对象可以同时具有两个角色。核心沿用 cloudwu/aoi 的移动阈值和候选对算法。[上游源码](https://github.com/cloudwu/aoi/tree/54660b509d6f91bba24d0307c565a5f355508812)

## 在 WSL 编译和测试

Ubuntu / Debian 使用系统的 Lua 5.4 解释器和开发头文件。在项目根目录运行以下命令。

```sh
sudo apt install build-essential lua5.4 liblua5.4-dev
make
make test
```

模块生成在 `src/luac/luaaoi.so`。可用 `LUA_INCLUDE` 和 `LUA` 指定其他安装位置的 Lua 5.4 头文件和解释器。运行自己的脚本时设置模块搜索路径。

```sh
LUA_CPATH='./src/luac/?.so;;' lua5.4 your_script.lua
```

## 使用

```lua
local aoi = require "luaaoi"
local scene = aoi.create(1024)
scene:register(function(events)
    for _, pair in ipairs(events) do
        print(pair[1], pair[2])  -- watcher ID, marker ID
    end
end)

scene:update(1, 0, 0, "w")
scene:update(2, 20, 0, "m")
scene:message()  -- 本批收到 {1, 2}
scene:remove(2)
scene:close()
```

- ID 为 0 到 0xfffffffe 的整数。每次 update 同时设置坐标和角色。
- message 推进一轮检测。角色变化或相对上次记录位置累计移动至少 15 个单位会重新匹配，因此同一对对象可以再次上报。候选对的小幅移动也可能触发事件。
- 每次 message 调用已注册的回调一次，空批次也会调用。容量默认为 1024，超出时保留本轮最后收集的事件。对象遍历顺序由实现决定。
- register(nil) 清除回调。回调可在当前协程内更新场景或关闭管理器，回调错误会传回 message 调用者。close 可重复调用，GC 也会释放资源。

`make clean` 删除构建产物。用 `lua5.4 src/lua/bench_simple.lua 12345` 运行固定种子的 5000 对象、50 帧基准。
