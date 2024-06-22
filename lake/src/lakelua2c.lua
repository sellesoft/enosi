--
--
-- Compile the lake.lua file into a luajit chunk that we can store and load from within the binary
--
--

local outpath = "src/generated/lakeluacompiled.h"


local compiled = string.dump(loadfile("src/lake.lua"))

local test = loadstring(compiled)

print(test())

io.open("fromlua", "w"):write(compiled)

local buffer = require "string.buffer"

local out = buffer.new()

out:put [[
static const int lake_lua_bytecode[] =
{]]

local compiled_length = #compiled

for i=1,compiled_length do
  if (i - 1) % 16 == 0 then
    out:put "\n "
  end

  out:putf("0x%02x, ", compiled:byte(i))
end

out:put([[

};
static const int lake_lua_bytecode_count = ]], compiled_length, ";")

local file = io.open(outpath, "w")

if not file then
  error("failed to open '"..outpath.."' for writing")
end

file:write(out:get())
file:close()
