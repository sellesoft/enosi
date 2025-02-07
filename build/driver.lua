--- 
--- Parser for command line args and implementations of subcommands.
---

local helpers = require "build.helpers"
local sys = require "build.sys"
local List = require "List"
local build = 
{
  obj = require "build.object",
  cmds = require "build.commands",
  flair = require "build.flair",
}

local driver = {}

driver.subcmds = {}

-- * --------------------------------------------------------------------------

--- Entry point for dealing with command line arguments and implementing the
--- behavior of 'subcommands' that we accept. The subcommand 'build' is the 
--- default, which will build all projects specified in user.lua.
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

-- * --------------------------------------------------------------------------

local createBuildCmds = function(proj)
  local cmds = {}

  local cpp_params = {} -- Shared with lpp's params.
  ---@type cmd.CppObj.Params | {}
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

	if sys.cfg.mode == "debug" and sys.cfg.asan then
	  params.address_sanitizer = true
	end

    cmds[build.obj.CppObj] = build.cmds.CppObj.new(params)

    cpp_params = params
  end

  ---@type cmd.LuaObj.Params | {}
  do local params = {}
    params.debug_info = true

    cmds[build.obj.LuaObj] = build.cmds.LuaObj.new(params)
  end

  ---@type cmd.LppObj.Params | {}
  do local params = {}
    -- TODO(sushi) handle projects specifying 'requires' if there is ever 
    --             a usecase.

    -- TODO(sushi) this should not be hardcoded like this but since I can't
    --             nicely add bin/ to my path via the nix flake I'm just 
    --             going to do this for now. Really really need to get rid of 
    --             nix.
    params.lpp = sys.root.."/bin/lpp"
    params.cpp = cpp_params

    cmds[build.obj.LppObj] = build.cmds.LppObj.new(params)
  end

  return cmds
end

-- * --------------------------------------------------------------------------

local resolveDirs = function(proj)
  local makeTask = function(dst, src)
    local task = lake.task(dst)
      :dependsOn(lake.task(src), proj.tasks.build_internal)
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
    proj.tasks.resolve_dirs:dependsOn(task)
  end

  local globDir = function(dir, spec)
    for file in lake.find(spec.glob):each() do
      local dst = dir.."/"..file
      local src = proj.root.."/"..spec.from.."/"..file
      makeTask(dst, src)
    end
  end

  local filterDir = function(dir, spec)
    for filter in List(spec.filters):each() do
      if type(filter) == "string" then
        local dst = dir.."/"..filter
        local src = proj.root.."/"..spec.from.."/"..filter
        makeTask(dst, src)
      elseif type(filter) == "table" then
        if not build.obj.BuildObject:isTypeOf(filter) then
          error("dir filter expects a BuildObject or string")
        end

        filter.proj = proj
        local src = proj.root.."/"..spec.from.."/"..filter:getTargetName()
        local dst = dir.."/"..filter:getTargetName()
        filter.task = makeTask(dst, src)

        -- Introduce the build object as public for this project.
        proj:trackBuildObject(true, filter)
      end
    end
  end

  for dirname,spec in pairs(proj.dirs) do
    local dir = proj:getAuxDirPath(dirname)
    if not spec.direct then
      if spec.to then
        dir = dir.."/"..spec.to
      end
      lake.mkdir(dir, {make_parents=true})
      lake.chdir(proj.root.."/"..spec.from)
      if spec.glob then
        globDir(dir, spec)
      elseif spec.filters then
        filterDir(dir, spec)
      end
    end
  end
end

-- * --------------------------------------------------------------------------

local loadEnabledProjects = function()
  for proj in List(sys.cfg.enabled_projects):each() do
    sys.getOrLoadProject(proj)
  end
end

-- * --------------------------------------------------------------------------

--- Builds projects. This is the default subcommand. 
---
--- Usage:
---   lake build [projname]
---
--- If 'projname' is not specified then the 'enabled_projects' cfg from 
--- user.lua is used to decide what projects we want to build. Otherwise
--- 'projname' is the name of a directory containing a 'project.lua' file
--- that we will use to build the project. This project does not have to be
--- specified in the 'enabled_projects' cfg to be built.
driver.subcmds.build = function(args)
  loadEnabledProjects()

  for proj in sys.projects.list:each() do
    lake.chdir(proj.root)
    local cmds = createBuildCmds(proj)

    local makeTasks = function(bobjs)
      for BObj,list in pairs(bobjs) do
        for bobj in list:each() do
          if bobj.defineTask then
            if cmds[BObj] then
              bobj:defineTask(cmds[BObj])
            else
              bobj:defineTask()
            end
          end
        end
      end
    end

    makeTasks(proj.bobjs.private)
    makeTasks(proj.bobjs.public)

    resolveDirs(proj)
  end

  lake.chdir(sys.root)
end

-- * --------------------------------------------------------------------------

--- Cleans projects. 
---
--- Usage:
---   lake clean [<projname> | all | external]
---
--- If none of the options are specified we use the 'enabled_projects' cfg
--- from user.lua to decide what projects to clean. This will only clean 
--- 'internal' projects. To clean all projects use 'all' or to only clean 
--- external projects use 'external'. Note that these two options still only
--- apply to those specified in 'enabled_projects'.
---
--- If <projname> is specified then only that project will be cleaned. This 
--- can be any kind of project.
---
driver.subcmds.clean = function(args)
  if #args == 0 then
    loadEnabledProjects()

    for proj in sys.projects.list:each() do
      if not proj.is_external then
        proj:clean()
      end
    end
  else
    local opt = args[1]
    if opt == "all" then
      loadEnabledProjects()

      for proj in sys.projects.list:each() do
        proj:clean()
      end
    elseif opt == "external" then
      loadEnabledProjects()

      for proj in sys.projects.list:each() do
        if proj.is_external then
          proj:clean()
        end
      end
    else
      local proj = sys.getOrLoadProject(opt)
        or error("failed to find project '"..opt.."' for cleaning")
      proj:clean()
    end
  end
end

-- * --------------------------------------------------------------------------

--- Publishes build objects specified by projects into the bin/ folder. 
--- Any build objects specified as published by a project will be copied 
--- there. Currently this SHOULD only be used for executables. Later on
--- we should expand this to support publishing other useful binaries but 
--- for exes are all we need.
---
--- Usage:
---   lake publish
--- 
--- Support for specifying projects will maybe be added later.
---
driver.subcmds.publish = function(args)
  if #args > 0 then
    error("'publish' does not take any arguments for the time being")
  end

  -- Set the build mode to release and start a build.
  sys.cfg.mode = "release"
  driver.subcmds.build()
  
  -- Iterate over all projects and copy their executables to bin.
  for name, proj in pairs(sys.projects.map) do
    for BObj, list in pairs(proj.bobjs.published) do
      for bobj in list:each() do
        local bobjpath = bobj:getOutputPath()
        local basename = helpers.getPathBasename(bobjpath)
        local target = sys.root.."/bin/"..basename
        
        lake.task(target)
          :workingDirectory(sys.root)
          :dependsOn(bobj.task)
          :cond(function(prereqs)
            if not lake.pathExists(target) then
              return true
            end
            
            local target_modtime = lake.modtime(target)
            for prereq in prereqs:each() do
              if lake.modtime(prereq) > target_modtime then
                return true
              end
            end
          end)
          :recipe(function()
            -- Remove any file that might already be there. This is done to 
            -- handle publishing lake, which is (at the time of writing) the
            -- program that will be running this script.
            --
            -- TODO(sushi) this probably wont work on Windows.
            if lake.pathExists(target) then
              lake.rm(target)
            end

            -- Copy the new file.
            lake.copy(target, bobjpath)

            build.flair.writeSuccessOnlyOutput(target)
          end)
      end
    end
  end
end

return driver
