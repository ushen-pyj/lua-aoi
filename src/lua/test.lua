local aoi = require "luaaoi"

local function batch(capacity)
    local scene = aoi.create(capacity)
    scene:update(7, 0, 0, "w")
    for id = 20, 39 do
        scene:update(id, 1, 1, "m")
    end

    local out = {}
    assert(not pcall(scene.message, scene))
    local ok, n = coroutine.resume(coroutine.create(function()
        return scene:message(out)
    end))
    assert(ok and n == capacity and #out == 2 * n)
    scene:close()
    scene:close()
    assert(not pcall(scene.message, scene, out))
    return out
end

local all = batch(20)
local last = batch(3)
local seen = {}
for i = 1, #all, 2 do
    local watcher, marker = all[i], all[i + 1]
    assert(watcher == 7 and marker >= 20 and marker <= 39)
    assert(not seen[marker])
    seen[marker] = true
end
for i = 1, #last do
    assert(last[i] == all[#all - #last + i])
end

local scene = aoi.create()
local out = {}
local marker = 0xfffffffe
scene:update(0, 0, 0, "w")
scene:update(marker, 10, 0, "m")
assert(scene:message(out) == 0 and #out == 0)

scene:update(marker, 9, 0, "m")
assert(scene:message(out) == 1 and #out == 2)
assert(out[1] == 0 and out[2] == marker)
assert(scene:message(out) == 0 and #out == 0)

scene:update(marker, 18, 0, "m")
assert(scene:message(out) == 0)
scene:remove(marker)
scene:update(marker, 1, 0, "wm")
assert(scene:message(out) == 1 and out[1] == 0 and out[2] == marker)
scene:update(0, 0, 0, "m")
assert(scene:message(out) == 1 and out[1] == marker and out[2] == 0)
scene:remove(marker)
assert(scene:message(out) == 0 and #out == 0)

for _, n in ipairs({0, -1, math.maxinteger}) do
    assert(not pcall(aoi.create, n))
end
for _, id in ipairs({-1, 0xffffffff, 0x100000000}) do
    assert(not pcall(scene.remove, scene, id))
    assert(not pcall(scene.update, scene, id, 0, 0, "w"))
end
assert(not pcall(scene.update, scene, 1, 0/0, 0, "w"))
assert(not pcall(scene.update, scene, 1, 0, 0, "w\0m"))
assert(not pcall(scene.message, scene, false))

local weak = setmetatable({scene}, { __mode = "v" })
scene = nil
collectgarbage()
collectgarbage()
assert(weak[1] == nil)
print("AOI tests passed")
