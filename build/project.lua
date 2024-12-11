---
--- A library of code that results in some build objects.
---

local lake = require "lake"
local Type = require "Type"
local List = require "list"

local Project = Type.make()

Project.new = function(name, wdir)
  local o = {}
  o.name = name
  o.wdir = wdir
  o.bobjs = 
  {
    int = List{},
    ext = List{},
  }
  return setmetatable(o, Project)
end




