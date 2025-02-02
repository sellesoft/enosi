--- 
--- Collection of commands that are used to build build objects.
---
--- Some Cmds take a table of parameters on creation and create an object
--- storing a command template to be filled in later via a call to 'complete'.
--- This allows us to build the static parts of a command once, to be filled
--- out with variable args like input and output files later.
---
--- This file is intended to be completely independent of the build system
--- as a whole, eg. it should never depend on any of its state. Do not 
--- require "build.sys" here, nor require anything else that might.
---
--- The reason being that this file is also used by the repo initialization
--- script, where the build system is not active yet because lake will have 
--- yet to be built.
---

---@alias List iro.List

local List = require "List"
local Type = require "Type"
local helpers = require "build.helpers"

local cmd = {}

---@class cmd.CppObj
--- Partially built command.
---@field partial iro.List
--- The compiler this command uses.
---@field compiler string
--- The parameters used to create this command.
---@field params cmd.CppObj.Params
cmd.CppObj = Type.make()

---@class cmd.CppObj.Params
--- The compiler to use.
---@field compiler string
--- The C++ std to use. Default: c++20
---@field std string
--- The optimization to use. Default is 'none'.
--- May be one of:
---   none
---   size
---   speed
---@field opt string
--- List of preprocessor defines given in the form:
---   { name, value }
---@field defines iro.List
--- If debug info should be emitted.
---@field debug_info boolean
--- Directories to search for includes.
---@field include_dirs iro.List
--- Disable building with C++'s builtin RTTI.
---@field nortti boolean
--- Force all symbols exposed to the dynamic table by default.
--- Default is false.
---@field export_all boolean
--- Compile with position independent code.
---@field pic boolean
--- Compile with address sanitizer enabled.
--- NOTE that if this is enabled for an object file, it must be enabled 
---      when linking it as well!
---@field address_sanitizer boolean

---@param params cmd.CppObj.Params
---@return cmd.CppObj
cmd.CppObj.new = function(params)
  local o = {}
  o.compiler = params.compiler
  o.params = params

  if o.compiler == "clang++" then
    o.partial = helpers.listBuilder(
      "clang++",
      "-c",
      cmd.CppObj.getIOIndependentFlags(params))
  else
    print(debug.traceback())
    error("unhandled compiler: "..o.compiler)
  end

  return setmetatable(o, cmd.CppObj)
end

cmd.CppObj.getDefineFlags = function(compiler, defines, list)
  if not defines then return end
  if "clang++" == compiler then
    for define in defines:each() do
      if define[2] then
        list:push("-D"..define[1].."="..define[2])
      else
        list:push("-D"..define[1])
      end
    end
  else
    error("unhandled compiler "..compiler)
  end
end

cmd.CppObj.getIncludeDirFlags = function(compiler, include_dirs, list)
  if not include_dirs then return end
  if "clang++" == compiler then
    for include_dir in include_dirs:each() do
      list:push("-I"..include_dir)
    end
  else
    error("unhandled compiler "..compiler)
  end
end

cmd.CppObj.getIOIndependentFlags = function(params)
  local o
  if params.compiler == "clang++" then
    o = helpers.listBuilder(
      params.pic and "-fPIC",
      "-std="..(params.std or "c++20"),
      params.nortti and "-fno-rtti",
      params.debug_info and "-g" ,
      params.address_sanitizer and "-fsanitize=address",
      not params.export_all and "-fvisibility=hidden",
      --"-fpatchable-function-entry=16",
      "-Wno-#warnings",
      "-Wno-switch",
      "-Wno-return-type-c-linkage",
      "-fmessage-length=80",
      "-fdiagnostics-absolute-paths",
	  "-D_DLL",
	  "-D_MT")

    o:push(helpers.switch(params.opt,
    {
      none = "-O0",
      size = "-Os",
      speed = "-O2",
    }) or error("invalid opt "..params.opt))

    cmd.CppObj.getDefineFlags("clang++", params.defines, o)

    for include_dir in params.include_dirs:each() do
      o:push("-I"..include_dir)
    end
  else
    error("unhandled compiler "..params.compiler)
  end
  return o
end

cmd.CppObj.complete = function(self, cfile, ofile)
  if "clang++" == self.compiler then
    return helpers.listBuilder(
      self.partial,
      cfile,
      "-o",
      ofile)
  end
end

---@class cmd.LuaObj
--- Partially built command.
---@field partial List
cmd.LuaObj = Type.make()

---@class cmd.LuaObj.Params
--- Whether to keep debug info.
---@field debug_info boolean

---@param params cmd.LuaObj.Params
cmd.LuaObj.new = function(params)
  local o = {}

  params.debug_info = params.debug_info or true

  o.partial = helpers.listBuilder(
    "luajit", 
    "-b",
    params.debug_info and "-g")

  return setmetatable(o, cmd.LuaObj)
end

cmd.LuaObj.complete = function(self, lfile, ofile)
  return helpers.listBuilder(
    self.partial,
    lfile,
    ofile)
end

--- Generate an obj file from an lpp file.
--- Note that this goes through two compilations, the first turning the lpp
--- file into a cpp file and the second turning that into an obj file, so make
--- sure both commands are run. This command's 'complete' function returns 
--- both the lpp and cpp commands.
---@class cmd.LppObj
---@field partial List
--- The cpp cmd that will be used to build the object file.
---@field cpp cmd.CppObj
cmd.LppObj = Type.make()

---@class cmd.LppObj.Params
--- The path to the lpp executable.
-- TODO(sushi) eventually this should be removed in favor of expecting lpp
--             to be on path just like everything else. This is blocked atm
--             by nix not making it easy to add a local folder to path in a
--             flake. Really need to get rid of that.
---
---@field lpp string
--- Require dirs.
---@field requires List
--- CppObj.Params that will be used to build an obj from the resulting cpp 
--- file.
---@field cpp cmd.CppObj.Params

---@param params cmd.LppObj.Params
---@return cmd.LppObj
cmd.LppObj.new = function(params)
  local o = {}

  local cargs = "--cargs="
  local flags = cmd.CppObj.getIOIndependentFlags(params.cpp):flatten()
  for carg in flags:each() do
    cargs = cargs..carg..","
  end

  local requires = List{}
  for require in List(params.requires):each() do
    requires:push "-R"
    requires:push(require)
  end
    
  o.partial = helpers.listBuilder(
    params.lpp,
    cargs,
    requires,
    -- little bit of cheating, really need to fix this somehow
    "-R", "src")

  o.cpp = cmd.CppObj.new(params.cpp)

  return setmetatable(o, cmd.LppObj)
end

--- Generates commands for both lpp->cpp and cpp->obj.
---
--- Path to input lpp file.
---@param input string
--- Path to output cpp file.
---@param cppout string
--- Path to output obj file.
---@param objout string
--- Optional path to a depfile to generate.
---@param depfile string?
--- Optional path to a metafile to generate.
---@param metafile string?
---@return List, List
cmd.LppObj.complete = function(self, input, cppout, objout, depfile, metafile)
  return 
    helpers.listBuilder(
      self.partial,
      input,
      "-o",
      cppout,
      metafile and { "-om", metafile },
      depfile and { "-D", depfile }),
    self.cpp:complete(cppout, objout)
end

--- Generate a depfile from a cpp file.
---@class cmd.CppDeps
---@field partial List
cmd.CppDeps = Type.make()

---@class cmd.CppDeps.Params
--- The compiler used for cpp.
---@field compiler string
--- A list of defines.
---@field defines List
--- A list of include directories.
---@field include_dirs List

---@param params cmd.CppDeps.Params
cmd.CppDeps.new = function(params)
  local o = {}
  o.compiler = params.compiler

  if "clang++" == params.compiler then
    o.partial = helpers.listBuilder(
      "clang++",
      "-MM",
      "-MG")

    cmd.CppObj.getDefineFlags("clang++", params.defines, o.partial)
    cmd.CppObj.getIncludeDirFlags("clang++", params.include_dirs, o.partial)
  else
    error("unhandled compiler "..params.compiler)
  end

  return setmetatable(o, cmd.CppDeps)
end

cmd.CppDeps.complete = function(self, cfile)
  if self.compiler == "clang++" then
    return helpers.listBuilder(
      self.partial,
      cfile)
  end
end

---@class cmd.Exe
---@field partial List
cmd.Exe = Type:make()

--- NOTE that this command applies to both executable files and shared library
--- files since the command is practically the same. I probably should have 
--- called this command Linker, but since this was written during a time where
--- I wanted there to be a command for every type of build object, it got named
--- something less clear instead.
--- 
-- TODO(sushi) rename this to Linker.
---
---@class cmd.Exe.Params
--- The linker to use.
---@field linker string
--- Libraries to link against. Currently these are wrapped in a group on 
--- Linux as I am too lazy to figure out what the proper linking order is
--- for LLVM. This will probably be fixed once I need to get LLVM building on 
--- Windows.
---@field shared_libs List
--- Static libs to link against. This is primarily useful when a library 
--- outputs both static and shared libs under the same name and the 
--- static lib is preferred.
---@field static_libs List
--- List of directories to search for libs in.
---@field libdirs List
--- If true, a shared library will be built.
---@field is_shared boolean
--- Whether to output debug information. This is probably only useful on Windows where
--- it intends to output a pdb.
---@field debug_info boolean
--- Link with address sanitizer enabled.
--- NOTE that if this is enabled, it must be enabled when compiling linked 
---      object files as well!
---@field address_sanitizer boolean

---@param params cmd.Exe.Params
---@return cmd.Exe
cmd.Exe.new = function(params)
  local o = {}
  o.linker = params.linker

  if "ld" == params.linker 
	 or "lld" == params.linker
     or "mold" == params.linker 
  then
    -- TODO(sushi) it sucks that we operate the linker through clang here, but 
    --             since I'm using nix to manage my build environment on Linux
    --             right now I've found it very difficult figuring out how to 
    --             properly construct the arguments for directly calling ld.
    --             At some point I would really like to drop nix for this 
    --             reason as well as it just making it difficult to understand
    --             my build environment in general. Ideally we would setup the
    --             init scripts to setup the build environments manually, but 
    --             properly getting external dependencies tracked and setup
    --             seems like it could be a nightmare.
    o.partial = helpers.listBuilder(
      "clang++",
      "-fuse-ld="..params.linker,
      params.is_shared and "-shared",
      params.address_sanitizer and "-fsanitize=address",
      params.debug_info and "-g")

    o.links = helpers.listBuilder(
      params.libdirs and params.libdirs:flatten():map(function(dir)
        return "-L"..dir
      end),
      "-Wl,--start-group",
      params.shared_libs and params.shared_libs:flatten():map(function(lib)
        return "-l"..lib
      end),
      params.static_libs and params.static_libs:flatten():map(function(lib)
        return "-l"..lib
      end),
      "-Wl,--end-group",
      "-Wl,-E",
	  "-lmsvcrt",
	  "-lucrt",
	  "-lversion",
	  "-lntdll")
      -- Tell the exe's dynamic linker to check the directory its in 
      -- for shared libraries.
      -- NOTE(sushi) this is disabled for now as I'm not actually using it 
      --             yet and it causes an issue in enosi's init script 
      --             since I'm using lua's os.execute which runs a command
      --             through the shell. This causes $ORIGIN to be expanded
      --             to whatever it is set to. When we come back to this we
      --             should probably just pull out iro's platform process
      --             stuff into some C code we can compile and dynamically
      --             load into the init script as its probably safer and 
      --             more portable.
      --"-Wl,-rpath,$ORIGIN")
  else
    error("unhandled linker "..params.linker)
  end

  return setmetatable(o, cmd.Exe)
end

cmd.Exe.complete = function(self, objs, out)
  return helpers.listBuilder(
    self.partial,
    objs,
    "-o",
    out,
    self.links)
end

---@class cmd.StaticLib.Params
--- The program to use.
---@field program string
--- The obj files.
---@field objs List
--- The name of the lib.
---@field name string

---@param params cmd.StaticLib.Params
cmd.StaticLib = function(params)
  if "ar" == params.program then
    return helpers.listBuilder(
      "ar",
      "rcs",
      "lib"..params.name..".a",
      params.objs)
  else
    error("unhandled program "..params.program)
  end
end

---@class cmd.Makefile
cmd.Makefile = Type.make()

---@class cmd.Makefile.Params

---@param params cmd.Makefile.Params
cmd.Makefile.new = function(params)
  return helpers.listBuilder("make", "-j") 
end

---@class cmd.CMake
cmd.CMake = Type.make()

---@class cmd.CMake.Params
--- What generator to use.
---@field generator string
--- Arguments passed to CMake.
---@field args iro.List
--- Path to the configuration directory.
---@field config_dir string

cmd.CMake.new = function(params)
  return helpers.listBuilder(
    "cmake",
    "-G",
    params.generator or 
      error("no generator specified"),
    params.config_dir,
    params.args
  )
end

return cmd
