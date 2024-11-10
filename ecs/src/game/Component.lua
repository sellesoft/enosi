local lpp = require "lpp"
local List = require "list"
local buffer = require "string.buffer"
local Type = require "Type"

local log = require "logger" ("comp", Verbosity.Info)

local comp = {}

comp.components =
{
  list = List{},
  map = {},
}

local Component = Type.make()
comp.Component = Component

Component.new = function(name)
  local o = {}
  o.name = name
  return setmetatable(o, Component)
end

comp.def = function(name, def)
  if comp.components.map[name] then
    log:fatal("a component with name ", name, " already exists\n")
    os.exit(1)
  end

  local c = Component.new(name)
  comp.components.map[name] = c
  comp.components.list:push(c)

  local start, stop = def:find("%b{}")
  if not start then
    log:fatal("expected a def")
    os.exit(1)
  end

  def = def:sub(start+1,stop-1)

  local buf = buffer.new()

  buf:put("struct ", name, " : public Component \n{\n", def)

  buf:put 
  [[
    b8 init();
  ]]

  buf:put("\n};\n")

  return buf:get()
end

comp.sort = function()
  table.sort(comp.components.list, function(a,b)
    return a.name < b.name
  end)
end

return comp
