-- 设置cpath以找到luaaoi模块
package.cpath = package.cpath .. ";./src/luac/?.so"

local aoi = require "luaaoi"
-- 配置参数
local NUM_OBJECTS = 5000        -- 测试对象数量
local WORLD_SIZE = 1000         -- 世界大小
local NUM_ITERATIONS = 50       -- 迭代次数
local MOVE_SPEED = 1.0          -- 移动速度

-- 对象管理
local objects = {}

-- 统计数据
local message_count = 0
local total_time = 0
local update_time = 0
local message_time = 0
local start_time, end_time

-- 初始化对象数据
local function init_objects()
    math.randomseed(tonumber(arg[1]) or 12345)
    
    for i = 1, NUM_OBJECTS do
        objects[i] = {
            id = i,
            -- 随机位置
            x = math.random() * WORLD_SIZE,
            y = math.random() * WORLD_SIZE,
            -- 随机速度
            vx = (math.random() * 2 - 1) * MOVE_SPEED,
            vy = (math.random() * 2 - 1) * MOVE_SPEED,
            -- 随机角色
            mode = math.random() > 0.5 and "w" or "m"
        }
        
        -- 部分对象同时是观察者和标记者
        if math.random() > 0.75 then
            objects[i].mode = "wm"
        end
    end
end

-- 更新对象位置
local function update_objects(manager)
    for i, obj in ipairs(objects) do
        -- 更新位置
        obj.x = obj.x + obj.vx
        obj.y = obj.y + obj.vy
        
        -- 边界处理（环绕）
        if obj.x < 0 then obj.x = obj.x + WORLD_SIZE end
        if obj.x > WORLD_SIZE then obj.x = obj.x - WORLD_SIZE end
        if obj.y < 0 then obj.y = obj.y + WORLD_SIZE end
        if obj.y > WORLD_SIZE then obj.y = obj.y - WORLD_SIZE end
        
        -- 更新AOI
        manager:update(obj.id, obj.x, obj.y, obj.mode)
    end
end

-- 运行基准测试
print("=== Lua AOI 简单基准测试 ===")
print("对象数量:", NUM_OBJECTS)
print("迭代次数:", NUM_ITERATIONS)
print("世界大小:", WORLD_SIZE)

-- 创建AOI管理器
local manager = aoi.create(100000)
local out = {}

-- 初始化对象
print("初始化对象...")
init_objects()

-- 添加到AOI空间
for _, obj in ipairs(objects) do
    manager:update(obj.id, obj.x, obj.y, obj.mode)
end

print("开始测试...")
start_time = os.clock()

for i = 1, NUM_ITERATIONS do
    -- 更新对象
    local update_start = os.clock()
    update_objects(manager)
    update_time = update_time + (os.clock() - update_start)
    
    -- 处理消息
    local message_start = os.clock()
    message_count = message_count + manager:message(out)
    message_time = message_time + (os.clock() - message_start)
    
    -- 进度报告
    if i % 10 == 0 then
        print(string.format("完成 %d/%d 次迭代", i, NUM_ITERATIONS))
    end
end

end_time = os.clock()
total_time = end_time - start_time

-- 打印结果
print("\n=== 测试结果 ===")
print("总事件数", message_count)
print(string.format("总运行时间: %.3f 秒", total_time))
print(string.format("对象更新时间: %.3f 秒 (%.1f%%)", update_time, update_time / total_time * 100))
print(string.format("消息处理时间: %.3f 秒 (%.1f%%)", message_time, message_time / total_time * 100))
print(string.format("平均帧率: %.1f FPS", NUM_ITERATIONS / total_time))
print(string.format("每秒平均处理对象数: %.0f", NUM_OBJECTS * NUM_ITERATIONS / total_time))
print(string.format("每帧平均更新时间: %.3f 毫秒", update_time * 1000 / NUM_ITERATIONS))
print(string.format("每帧平均消息处理时间: %.3f 毫秒", message_time * 1000 / NUM_ITERATIONS))

print("\n基准测试完成!")
