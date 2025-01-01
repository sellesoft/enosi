local sys = require "build.sys"
local helpers = require "build.helpers"
local bobj = require "build.object"

local luajit = sys.getLoadingProject()

-- Mark as external, which prevents the build system trying to automatically
-- handle some things with this project, eg. this prevents it from being 
-- cleaned unless the user has specifically requested that we clean 'all' 
-- or 'external' projects.
luajit.is_external = true

luajit.report.dir.include
{
  from = "src/src",
  to = "luajit",

  filters = 
  {
    "lua.h",
    "lauxlib.h",
    "lualib.h",
    "luajit.h",
    "luaconf.h",
  }
}

luajit.report.dir.lib
{
  from = "src/src",

  filters = 
  {
    bobj.StaticLib "luajit"
  }
}

luajit.report.Makefile "src/src" 

luajit.cleaner = function(self)
  lake.task("clean luajit")
    :cond(function() return true end)
    :workingDirectory(self.root.."/src/src")
    :recipe(function()
      lake.cmd({"make", "clean"})
    end)
end
