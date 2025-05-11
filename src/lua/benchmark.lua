-- 设置cpath以找到luaaoi模块
package.cpath = package.cpath .. ";./src/luac/?.so"

local aoi = require "luaaoi"

-- 配置参数
local NUM_OBJECTS = 10000        -- 测试对象数量
local WORLD_SIZE = 1000          -- 世界大小
local NUM_ITERATIONS = 100       -- 迭代次数
local MOVE_SPEED = 1.0           -- 移动速度
local MAX_PAIRS = 1000000        -- 最大对交互数

-- 对象管理
local objects = {}

-- 统计数据
local stats = {
    update_time = 0,
    message_time = 0,
    total_detected = 0,
    max_detected = 0,
}

-- AOI事件回调函数
local function on_aoi_event(pairs)
    local count = #pairs
    stats.total_detected = stats.total_detected + count
    if count > stats.max_detected then
        stats.max_detected = count
    end
end

-- 高精度时间测量函数
local has_socket, socket = pcall(require, "socket")
local get_time
if has_socket then
    -- 使用LuaSocket提供的高精度计时
    get_time = socket.gettime
else
    -- 备选方案
    get_time = function()
        local clock = os.clock()
        if clock then return clock
        else return os.time() end
    end
end

-- 初始化对象数据
local function init_objects()
    -- 确保随机性
    math.randomseed(os.time())
    
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
            mode = "m"  -- 默认为标记者
        }
        
        -- 一半概率设为观察者
        if math.random() > 0.5 then
            objects[i].mode = "w"
        end
        
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

-- 基准测试的计时器函数
local function measure(func, ...)
    local start = get_time()
    func(...)
    local finish = get_time()
    return finish - start
end

-- 运行基准测试
local function run_benchmark()
    print("创建AOI管理器...")
    local manager = aoi.create(MAX_PAIRS)
    manager:register(on_aoi_event)
    
    print("初始化", NUM_OBJECTS, "个对象...")
    init_objects()
    
    -- 初始添加所有对象到AOI
    print("将对象添加到AOI空间...")
    for _, obj in ipairs(objects) do
        manager:update(obj.id, obj.x, obj.y, obj.mode)
    end
    
    -- 运行测试循环
    print("开始测试", NUM_ITERATIONS, "次迭代...")
    
    -- 预热 - 忽略前10次迭代的计时
    for i = 1, 10 do
        update_objects(manager)
        manager:message()
    end
    
    for i = 1, NUM_ITERATIONS do
        -- 更新所有对象
        local update_elapsed = measure(update_objects, manager)
        stats.update_time = stats.update_time + update_elapsed
        
        -- 处理AOI消息
        local message_elapsed = measure(function() manager:message() end)
        stats.message_time = stats.message_time + message_elapsed
        
        -- 每10次迭代报告一次进度
        if i % 10 == 0 then
            print(string.format("已完成 %d/%d 次迭代 (更新: %.3f毫秒, 消息: %.3f毫秒)", 
                i, NUM_ITERATIONS, update_elapsed*1000, message_elapsed*1000))
        end
    end
    
    -- 打印结果
    print("\n--- 性能统计 ---")
    print(string.format("对象数量: %d", NUM_OBJECTS))
    print(string.format("迭代次数: %d", NUM_ITERATIONS))
    print(string.format("平均更新时间: %.3f 毫秒/帧", stats.update_time * 1000 / NUM_ITERATIONS))
    print(string.format("平均消息处理时间: %.3f 毫秒/帧", stats.message_time * 1000 / NUM_ITERATIONS))
    print(string.format("总检测到的交互数: %d", stats.total_detected))
    print(string.format("单帧最大交互数: %d", stats.max_detected))
    print(string.format("平均每帧交互数: %.2f", stats.total_detected / NUM_ITERATIONS))
    print(string.format("总运行时间: %.3f 秒", (stats.update_time + stats.message_time)))
    
    -- 每秒可处理的对象数量
    local objects_per_second = NUM_OBJECTS * NUM_ITERATIONS / stats.update_time
    print(string.format("每秒可处理对象更新数: %.0f", objects_per_second))
    
    -- 每秒可处理的消息数量
    local messages_per_second = NUM_ITERATIONS / stats.message_time
    print(string.format("每秒可处理消息帧数: %.0f", messages_per_second))
    
    return manager
end

-- 热点区域测试 - 模拟大量对象聚集在一起的情况
local function run_hotspot_test()
    print("\n--- 热点区域测试 ---")
    print("模拟大量对象聚集在一起的情况...")
    
    -- 重置对象和统计
    objects = {}
    stats = {
        update_time = 0,
        message_time = 0,
        total_detected = 0,
        max_detected = 0,
    }
    
    local manager = aoi.create(MAX_PAIRS)
    manager:register(on_aoi_event)
    
    -- 创建聚集区域
    local HOTSPOT_RADIUS = 20
    local HOTSPOT_CENTER_X = WORLD_SIZE / 2
    local HOTSPOT_CENTER_Y = WORLD_SIZE / 2
    
    -- 观察者聚集在另一边
    local WATCHER_CENTER_X = HOTSPOT_CENTER_X + HOTSPOT_RADIUS * 1.5
    local WATCHER_CENTER_Y = HOTSPOT_CENTER_Y
    
    -- 初始化对象 - 一半对象聚集在热点区域
    for i = 1, NUM_OBJECTS do
        if i <= NUM_OBJECTS / 2 then
            -- 热点区域的标记者
            local angle = math.random() * 2 * math.pi
            local distance = math.random() * HOTSPOT_RADIUS
            objects[i] = {
                id = i,
                x = HOTSPOT_CENTER_X + math.cos(angle) * distance,
                y = HOTSPOT_CENTER_Y + math.sin(angle) * distance,
                vx = (math.random() * 2 - 1) * MOVE_SPEED * 0.2, -- 慢速移动
                vy = (math.random() * 2 - 1) * MOVE_SPEED * 0.2,
                mode = "m"
            }
        else
            -- 观察者聚集区
            local angle = math.random() * 2 * math.pi
            local distance = math.random() * HOTSPOT_RADIUS
            objects[i] = {
                id = i,
                x = WATCHER_CENTER_X + math.cos(angle) * distance,
                y = WATCHER_CENTER_Y + math.sin(angle) * distance,
                vx = (math.random() * 2 - 1) * MOVE_SPEED * 0.2,
                vy = (math.random() * 2 - 1) * MOVE_SPEED * 0.2,
                mode = "w"
            }
        end
    end
    
    -- 初始添加所有对象到AOI
    for _, obj in ipairs(objects) do
        manager:update(obj.id, obj.x, obj.y, obj.mode)
    end
    
    -- 运行测试循环
    print("开始测试", NUM_ITERATIONS, "次迭代...")
    
    -- 预热 - 忽略前10次迭代的计时
    for i = 1, 10 do
        update_objects(manager)
        manager:message()
    end
    
    for i = 1, NUM_ITERATIONS do
        -- 更新所有对象
        local update_elapsed = measure(update_objects, manager)
        stats.update_time = stats.update_time + update_elapsed
        
        -- 处理AOI消息
        local message_elapsed = measure(function() manager:message() end)
        stats.message_time = stats.message_time + message_elapsed
        
        -- 每10次迭代报告一次进度
        if i % 10 == 0 then
            print(string.format("已完成 %d/%d 次迭代", i, NUM_ITERATIONS))
        end
    end
    
    -- 打印结果
    print("\n--- 热点区域性能统计 ---")
    print(string.format("对象数量: %d", NUM_OBJECTS))
    print(string.format("平均更新时间: %.3f 毫秒/帧", stats.update_time * 1000 / NUM_ITERATIONS))
    print(string.format("平均消息处理时间: %.3f 毫秒/帧", stats.message_time * 1000 / NUM_ITERATIONS))
    print(string.format("总检测到的交互数: %d", stats.total_detected))
    print(string.format("单帧最大交互数: %d", stats.max_detected))
    print(string.format("平均每帧交互数: %.2f", stats.total_detected / NUM_ITERATIONS))
    
    return manager
end

-- 额外运行带有不同配置的测试
local function run_scaling_test()
    local sizes = {1000, 5000, 10000, 20000}
    local results = {}
    
    print("\n--- 扩展性测试 ---")
    print("测试不同数量对象的性能...")
    
    for _, size in ipairs(sizes) do
        -- 重置对象和统计
        NUM_OBJECTS = size
        objects = {}
        stats = {
            update_time = 0,
            message_time = 0,
            total_detected = 0,
            max_detected = 0,
        }
        
        print(string.format("\n[测试对象数量: %d]", size))
        run_benchmark()
        
        -- 保存结果
        results[size] = {
            update_time = stats.update_time * 1000 / NUM_ITERATIONS,
            message_time = stats.message_time * 1000 / NUM_ITERATIONS,
            avg_detected = stats.total_detected / NUM_ITERATIONS
        }
    end
    
    -- 打印扩展性结果
    print("\n--- 扩展性结果摘要 ---")
    print("对象数量\t更新时间(毫秒)\t消息时间(毫秒)\t平均交互数")
    for _, size in ipairs(sizes) do
        local r = results[size]
        print(string.format("%d\t%.3f\t\t%.3f\t\t%.2f", 
            size, r.update_time, r.message_time, r.avg_detected))
    end
end

-- 密度测试 - 测试不同世界大小下的性能
local function run_density_test()
    local original_size = WORLD_SIZE
    local sizes = {500, 1000, 2000, 5000}
    
    print("\n--- 密度测试 ---")
    print("测试不同世界大小的性能（固定对象数量）...")
    
    for _, world_size in ipairs(sizes) do
        WORLD_SIZE = world_size
        objects = {}
        stats = {
            update_time = 0,
            message_time = 0,
            total_detected = 0,
            max_detected = 0,
        }
        
        print(string.format("\n[测试世界大小: %d]", world_size))
        run_benchmark()
    end
    
    -- 恢复原始设置
    WORLD_SIZE = original_size
end

-- 运行主基准测试
print("=== Lua AOI 基准测试 ===")
local manager = run_benchmark()

-- 询问是否运行额外测试
print("\n是否运行热点测试? (y/n)")
local input = io.read()
if input == "y" then
    run_hotspot_test()
end

print("\n是否进行扩展性测试? (y/n)")
input = io.read()
if input == "y" then
    run_scaling_test()
end

print("\n是否进行密度测试? (y/n)")
input = io.read()
if input == "y" then
    run_density_test()
end

print("\n基准测试完成!") 