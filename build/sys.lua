---
--- The build system underlying enosi.
---

local log = require "Logger" ("build.sys", Verbosity.Notice)

local helpers = require "build.helpers"
local List = require "List"
local json = require "JSON"

local build_start = lake.getMonotonicClock()
lake.registerFinalCallback(function(success)
  local flair = require "build.flair"
  io.write("build ")
  if success then
    io.write(flair.green, "succeeded ", flair.reset)
  else
    io.write(flair.red, "failed ", flair.reset)
  end
  io.write("in ", (lake.getMonotonicClock() - build_start)/1000000, "s\n")

  for cb in require "build.driver" .finalCallbacks:each() do
    cb(success)
  end
end)

-- Loaded in run(), so can only be used inside of functions here.
local Project

local sys = {}

sys.log = log

sys.projects = 
{
  map = {},
  list = List{},
}

sys.compile_commands = List{}

--- Tracks a command for a compile_commands.json
sys.trackCompileCommandsCmd = function(dir, file, cmd)
  sys.compile_commands:push
  {
    directory = dir,
    file = file,
    arguments = lake.flatten(cmd),
  }
end

sys.root = lake.cwd()
package.path = package.path..";"..sys.root.."/?.lua"

-- Prepend our standard directories for tools to PATH.
local win_root = sys.root:gsub("/", "\\")
local PATH = 
  win_root.."\\bin;"..
	win_root.."\\llvm\\llvm_build\\Release\\Release\\bin;"..
  lake.getEnvVar("PATH")
if not lake.setEnvVar("PATH", PATH) then
  error("failed to set PATH")
end
           
sys.os = lake.os()

sys.Error = helpers.enum(
  "ProjDepCycle",
  "NonExistantProjDir",
  "NonExistantProjFile",
  "FailedToLoadProjFile",
  "ProjFileReturnedNothing",
  "ProjFileErrored")

require "build.cfg" (sys)

lake.setMaxJobs(sys.cfg.max_jobs)

local load_stack = List{}

sys.getLoadingProject = function()
  return load_stack:last()
end

sys.getOrLoadProject = function(name)
  local existing = sys.projects.map[name]
  if existing then
    return existing
  end

  local cycle
  for loading in load_stack:each() do
    if cycle then
      cycle = cycle.."  "..loading.." -> \n"
    else
      if loading.name == name then
        cycle = "  "..name.." -> \n"
      end
    end
  end

  if cycle then
    cycle = cycle.."  "..name.."\n"
    log:fatal("project dependency cycle detected!\n", cycle, "\n")
    error(sys.Error.ProjDepCycle)
  end

  local return_dir = sys.root

  local loading_proj = load_stack:last()
  if loading_proj then
    return_dir = loading_proj.root
  end

  lake.chdir(sys.root)

  if not lake.pathExists(name) then
    log:fatal(
      "requested to load project '", name, "', but there is no directory ",
      sys.root,"/",name,"\n")
    error(sys.Error.NonExistantProjDir)
  end

  if not lake.pathExists(name.."/project.lua") then
    log:fatal(
      "requested to load project '", name, "', but there is no project.lua ",
      "inside of ", sys.root, "/", name, "\n")
    error(sys.Error.NonExistantProjFile)
  end

  local loaded, err = loadfile(name.."/project.lua")
  if not loaded then
    log:fatal("failed to load ",name,"/project.lua:\n", err, "\n")
    error(sys.Error.FailedToLoadProjFile)
  end

  lake.chdir(name)

  local proj = Project.new(name, sys.root.."/"..name)
  load_stack:push(proj)

  local result, err = xpcall(loaded, function(err)
    log:fatal(debug.traceback(2), "\n", err, "\n")  
  end)

  if not result then
    log:fatal(
      "project file ",name,"/project.lua failed to run:\n",
      err, "\n")
    error(sys.Error.ProjFileErrored)
  end

  load_stack:pop()

  sys.projects.map[name] = proj
  sys.projects.list:push(proj)

  log:info("loaded project ", name, "\n")

  if name == "iro" then
    -- util.dumpValue(proj, 9)
  end

  lake.chdir(return_dir)

  return proj
end

sys.isProjectEnabled = function(name)
  for proj in List(sys.cfg.enabled_projects):each() do
    if proj == name then
      return true
    end
  end
  return false
end

sys.run = function()
  Project = require "build.project"

  require "build.driver" .run()

  if sys.cfg.mode == "debug" then
    -- If in debug, output a compile_commands.json
    local json = require "JSON"
    local file = io.open("compile_commands.json", "w")
    if not file then
      error("failed to open compile_commands.json")
    end
    file:write(json.encode(sys.compile_commands))
    file:close()
  end
end

return sys
