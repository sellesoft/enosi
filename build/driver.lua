--- 
--- Parser for command line args and implementations of subcommands.
---

local helpers = require "build.helpers"
local util = require "util"
local sys = require "build.sys"
local List = require "list"
local build = 
{
  obj = require "build.object",
  cmds = require "build.commands",
}

local driver = {}

driver.subcmds = {}

driver.run = function()
  local args = lake.cliargs

  if args:isEmpty() then
    driver.subcmds.build()
  else
    local subcmd = driver.subcmds[args[1]]
    if not subcmd then
      sys.log:fatal("unknown subcommand '",subcmd,"'")
      os.exit(1)
    end
    return (subcmd(args:sub(2)))
  end
end

local createBuildCmds = function(proj)
  local cmds = {}

  ---@type cmd.CppObj.Params
  do local params = {}
    params.compiler = sys.cfg.cpp.compiler

    params.defines = List{}
    proj:gatherDefines(params.defines)

    params.include_dirs = List{ "src" }
    proj:gatherDirs("include", params.include_dirs)

    params.nortti = true

    if sys.cfg.mode == "debug" then
      params.opt = "none"
      params.debug_info = true
    else
      params.opt = "speed"
    end

    cmds[build.obj.CppObj] = build.cmds.CppObj.new(params)
  end

  ---@type cmd.LuaObj.Params
  do local params = {}
    params.debug_info = true

    cmds[build.obj.LuaObj] = build.cmds.LuaObj.new(params)
  end

  ---@type cmd.Exe.Params
  do local params = {}
    params.linker = sys.cfg.cpp.linker
    params.libdirs = List{}
    proj:gatherDirs("lib", params.libdirs)
    
    params.static_libs = List{}
    proj:gatherBuildObjects(build.obj.StaticLib, function(bobj)
      params.static_libs:push(bobj.name)
    end)

    params.shared_libs = List{}
    proj:gatherBuildObjects(build.obj.SharedLib, function(bobj)
      params.shared_libs:push(bobj.name)
    end)

    util.dumpValue(params.static_libs, 3)

    cmds[build.obj.Exe] = build.cmds.Exe.new(params)
  end

  return cmds
end

local resolveDirs = function(proj)
  local makeTask = function(dst, src)
    lake.task(dst)
      :dependsOn(lake.task(src))
      :cond(function()
        if not lake.pathExists(dst) then
          return true
        end

        if lake.modtime(src) > lake.modtime(dst) then
          return true
        end
      end)
      :recipe(function()
        lake.mkdir(helpers.getPathDirname(dst), { make_parents = true})
        lake.copy(dst, src)
      end)
  end

  local globDir = function(dir, spec)
    for file in lake.find(spec.glob):each() do
      local dst = dir.."/"..file
      local src = proj.wdir.."/"..spec.from.."/"..file
      makeTask(dst, src)
    end
  end

  local filterDir = function(dir, spec)
    for filter in List(spec.filters):each() do
      if type(filter) == "string" then
        local dst = dir.."/"..filter
        local src = proj.wdir.."/"..spec.from.."/"..filter
        makeTask(dst, src)
      elseif type(filter) == "table" then
        if not build.obj.BuildObject:isTypeOf(filter) then
          error("dir filter expects a BuildObject or string")
        end

        filter:assignMount(dir)
        local src = proj.wdir.."/"..spec.from.."/"..filter:getTargetName()
        local dst = filter:formAbsolutePath()
        makeTask(dst, src)

        -- Introduce the build object as public for this project.
        proj:trackBuildObject(true, filter)
      end
    end
  end

  for dirname,spec in pairs(proj.dirs) do
    local dir = proj.wdir.."/"..dirname
    if spec.to then
      dir = dir.."/"..spec.to
    end
    lake.mkdir(dir)
    lake.chdir(proj.wdir.."/"..spec.from)
    if spec.glob then
      globDir(dir, spec)
    elseif spec.filters then
      filterDir(dir, spec)
    end
  end
end

driver.subcmds.build = function(args)
  for proj in List(sys.cfg.enabled_projects):each() do
    sys.getOrLoadProject(proj)
    -- Create the tasks.
    -- proj.task = lake.task(proj.name)
  end

  for proj in sys.projects.list:each() do
    local name = proj.name

    local cmds = createBuildCmds(proj)

    local makeTasks = function(bobjs)
      for BObj,list in pairs(bobjs) do
        for bobj in list:each() do
          if not bobj.is_link
             and cmds[BObj]
             and bobj.makeTask 
          then
            bobj:makeTask(cmds[BObj], proj)
          end
        end
      end
    end

    makeTasks(proj.bobjs.private)
    makeTasks(proj.bobjs.public)

    resolveDirs(proj)
  end
end

return driver
