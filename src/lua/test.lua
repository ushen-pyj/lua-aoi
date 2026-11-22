local aoi = require "luaaoi"

local function new_scene(capacity)
    local m = aoi.create(capacity)
    m:update(7, 0, 0, "w")
    for id = 20, 39 do
        m:update(id, 1, 1, "m")
    end
    return m
end

local function batch(capacity)
    local m = new_scene(capacity)
    local result
    m:register(function(events) result = events end)
    assert(coroutine.resume(coroutine.create(function() m:message() end)))
    m:close()
    m:close()
    assert(not pcall(m.message, m))
    return result
end

local all = batch(20)
local last = batch(3)
assert(#all == 20 and #last == 3)
local seen = {}
for _, event in ipairs(all) do
    assert(event[1] == 7 and event[2] >= 20 and event[2] <= 39)
    assert(not seen[event[2]])
    seen[event[2]] = true
end
for i, event in ipairs(last) do
    assert(event[1] == 7 and event[2] == all[17 + i][2])
end

local m = aoi.create()
local events
m:register(function(t) events = t end)
m:update(0, 0, 0, "w")
m:update(0xfffffffe, 1000, 0, "m")
m:message()
assert(#events == 0)
m:update(0xfffffffe, 0, 0, "m")
m:message()
assert(#events == 1 and events[1][1] == 0 and events[1][2] == 0xfffffffe)
m:remove(0xfffffffe)
m:message()
assert(#events == 0)
m:update(0xfffffffe, 0, 0, "m")
m:message()
assert(#events == 1)

for _, n in ipairs({0, -1, math.maxinteger}) do
    assert(not pcall(aoi.create, n))
end
for _, id in ipairs({-1, 0xffffffff, 0x100000000}) do
    assert(not pcall(m.remove, m, id))
    assert(not pcall(m.update, m, id, 0, 0, "w"))
end
assert(not pcall(m.update, m, 1, 0/0, 0, "w"))
assert(not pcall(m.update, m, 1, 0, 0, "w\0m"))
assert(not pcall(m.register, m, true))
m:register(function() error("callback failed") end)
local ok, err = pcall(m.message, m)
assert(not ok and err:find("callback failed", 1, true))
m:register(nil)
m:message()
m:close()

local weak = setmetatable({}, { __mode = "v" })
do
    local owner = aoi.create()
    owner:register(function() return owner end)
    weak[1] = owner
end
collectgarbage()
collectgarbage()
assert(weak[1] == nil)

-- A cached pair crosses the strict radius on a sub-threshold move.
local scene = aoi.create()
local hits
scene:register(function(batch) hits = batch end)
scene:update(1, 0, 0, "w")
scene:update(2, 30, 0, "m")
scene:message()
assert(#hits == 0)
scene:update(2, 29, 0, "m")
scene:message()
assert(#hits == 1 and hits[1][2] == 2)
scene:message()
assert(#hits == 0)
scene:update(2, 50, 0, "m")
scene:message()
scene:remove(2)
scene:update(2, 1, 0, "wm")
scene:message()
assert(#hits == 1 and hits[1][1] == 1 and hits[1][2] == 2)

-- The callback can replace itself, dispatch again and close its owner.
scene:register(function(outer)
    scene:register(function(inner)
        assert(#inner == 0)
        scene:close()
    end)
    scene:message()
    assert(#outer == 1 and outer[1][1] == 2 and outer[1][2] == 1)
end)
scene:update(1, 0, 0, "m")
scene:message()
scene:close()
print("AOI tests passed")
