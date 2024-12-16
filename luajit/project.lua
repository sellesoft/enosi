local sys = require "build.sys"
local helpers = require "build.helpers"
local bobj = require "build.object"

local luajit = sys.getLoadingProject()

luajit.report.dir.include
{
  from = "src/src",

  filters = 
  {
    "lua.h",
    "lauxlib.h",
    "lualib.h",
  }
}

luajit.report.dir.lib
{
  from = "src/src",

  filters = 
  {
    "static:luajit"
  }
}

luajit.report.Makefile("src", 
{
  bobj.StaticLib "src/src/luajit"
})

