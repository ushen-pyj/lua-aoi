package.cpath = package.cpath .. ";./src/luac/?.so"

local aoi = require "luaaoi"

local function on_aoi_event(watcher)
    for _, v in ipairs(watcher) do
        print(string.format("AOI事件: 观察者 %d 发现标记者 %d", v[1], v[2]))
    end
end

local test = function(manager)
    manager:register(on_aoi_event)

    manager:update(1, 0, 0, "wm")
    manager:update(2, 10, 0, "wm")
    manager:update(3, 100, 100, "m")
    manager:message()

    os.execute("sleep 1")

    print("更新对象2的位置到(5, 0)")
    manager:update(2, 5, 0, "wm")
    manager:message()
    print("msg end")

    os.execute("sleep 1")

    print("移动对象3到(1, 1)")
    manager:update(3, 1, 1, "m")
    manager:message()
    print("msg end")

    os.execute("sleep 1")

    print("删除对象2")
    manager:remove(2)
    manager:message()

    os.execute("sleep 1")

    print("测试完成")
end

local manager = aoi.create(100000)
test(manager)

local manager2 = aoi.create(100000)
test(manager2)
