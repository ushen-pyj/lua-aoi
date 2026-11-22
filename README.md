```sh
sudo apt install build-essential lua5.4 liblua5.4-dev
git submodule update --init -- aoi
make
make test
export LUA_CPATH='./src/luac/?.so;;'
```

```lua
local scene = require("luaaoi").create()
local out = {}

scene:update(1, 0, 0, "w")
scene:update(2, 8, 0, "m")

for i = 1, scene:message(out) do
    print(out[2 * i - 1], out[2 * i])
end
```
