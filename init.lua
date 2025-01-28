--- Build lake based on what platform we are running on.

local build_cmds = require "build.commands"
local List = require "List"
local Type = require "Type"

local writeSeparator = function()
  io.write
[[
 - ========================================================================= -

]]
end 

local getResponse = function()
  io.write "> "
  return io.read():sub(1,1)
end

local exec = function(...)
  local cmd = ""
  for _,arg in ipairs{...} do
    cmd = cmd..arg
  end

  io.write("\n$ ", cmd, "\n")

  return os.execute(cmd)
end

local function flattenList(l)
  local o = List{}

  local function recur(l)
    for elem in l:each() do
      if List == Type.of(elem) then
        recur(elem)     
      else
        o:push(elem)
      end
    end
  end

  recur(l)

  return o
end

local execBuildCmd = function(args)
  local cmd = ""
  for arg in flattenList(args):each() do
    cmd = cmd..arg.." "
  end

  io.write("\n$ ", cmd, "\n")

  return os.execute(cmd)
end

writeSeparator()

io.write
[[
Throughout this script we will try to download things.

Respond 'n' if this is not okay with you or if this is impossible in your 
situation.

By default, the script will stop before each download to show you what is 
being downloaded, why, and the link it will be downloaded from. We also 
ask you if you are okay with downloading the thing, allowing you to stop
the script if you don't trust something. 

Everything downloaded will be placed in ./tmp and this directory will
be deleted after the script terminates (whether the reason be success or not).

You may choose to automatically allow any download by responing 'a'.

Otherwise to run normally, use 'y'.

'y' - Run normally.
'a' - Run and ignore download messages.
'n' - Cancel running the script.

]]

local response = getResponse()

local mode = "normal"
if response == "a" then
  mode = "ignore"
elseif response ~= "y" then
  io.write "Goodbye!\n"
  os.exit(0)
end

local platform = arg[1]

local copyFile = function(src, dst)
  local src_file = io.open(src, "rb")
  local dst_file = io.open(dst, "wb")
  dst_file:write(src_file:read("*a"))
  src_file:close()
  dst_file:close()
end
-- if platform == "linux" then
--   copyFile = function(src, dst)
--     if 0 ~= exec("cp ",src.." ",dst) then
--       error("failed to copy "..src.." to "..dst)
--     end
--   end
-- elseif platform == "windows" then
--   copyFile = function(src, dst)
--     if 0 ~= exec("copy ",src.." ",dst) then
--       error("failed to copy "..src.." to "..dst)
--     end
--   end
-- else
--   error("unhandled os")
-- end

local tryDownload = function(name, link, reason)
  if mode ~= "ignore" then
    io.write("\n")
    writeSeparator()
    io.write(
      "I would like to download ", name, "\n",
      "  This will be downloaded from:\n", 
      "    ", link, "\n",
      "  I need this for: \n",
      "    ", reason, "\n\n",
      "  Is this ok? (y/N)\n\n")

    if "y" ~= getResponse() then
      io.write "\n\nGoodbye!\n"
      os.exit(1)
    end
  end

  io.write "\n"
  writeSeparator()

  io.write("Downloading ", name, "...\n")

  local result = exec("curl -o tmp/",name,".download -L ",link)
  if result ~= 0 then
    error("failed to download '"..name.."' from "..link.."!")
  end
end

local lfs_link = 
  "https://github.com/lunarmodules/luafilesystem/archive/refs/tags/"..
  "v1_8_0.tar.gz"

tryDownload("lfs", lfs_link, "navigating the file system easily")

if 0 ~= exec("tar -xf tmp/lfs.download -C tmp") then
  error "failed to extract lua filesystem!"
end

io.write("Compiling lfs...\n")

if 0 ~= exec("cd tmp/luafilesystem-1_8_0 && ",
   "clang -shared src/lfs.c ",
   "-o ../lfs.so ",
   "-I../../luajit/src/src ", 
   "-L../../luajit/src/src ",
   "-D_CRT_SECURE_NO_WARNINGS ",
   "-lluajit ",
   "-llua51 ") then
  error "failed to compile lua filesystem!"
end

package.cpath = package.cpath..";./tmp/?.so"

local lfs = require "lfs"

local cd = function(path)
  io.write("\n--> cd ", path, "\n\n")
  lfs.chdir(path)
end

local root = lfs.currentdir()

-- if platform == "windows" then
--   -- Try to make the path more 'normal' by removing the prefix
--   -- and normalizing \ to /
--   root = root:sub(3):gsub("\\", "/")
-- end

local build_dir = root.."/tmp/build"
lfs.mkdir(build_dir)

local iro_build = build_dir.."/iro"
lfs.mkdir(iro_build)
lfs.mkdir(iro_build.."/src")

local iro_include = iro_build.."/include"
lfs.mkdir(iro_include)
lfs.mkdir(iro_include.."/iro")

local lake_build = build_dir.."/lake"
lfs.mkdir(lake_build)

local function walkDir(path, f)
  for ent in lfs.dir(path) do
    if ent ~= ".." and ent ~= "." then
      local full = path.."/"..ent
      local attr = lfs.attributes(full)
      if attr.mode == "file" then
        f(full)
      elseif attr.mode == "directory" then
        walkDir(full, f)
      end
    end
  end
end

local objext
if platform == "linux" then
	objext = ".o"
else
	objext = ".obj"
end

local function getSourceToObj(path, bdir, c_to_o, l_to_o)
  walkDir(path, function(path)
    if path:match("%.cpp$") then
      c_to_o:push{path, bdir.."/"..path..objext}
    elseif l_to_o and path:match("%.lua$") then
      l_to_o:push{path, bdir.."/"..path..objext}
    end
    local odir = bdir.."/"..path:match("(.*)/")
    lfs.mkdir(odir)
  end)
end

local function compileObj(src_to_obj, cmd)
  for v in src_to_obj:each() do
    if 0 ~= execBuildCmd(cmd:complete(v[1], v[2]))  then
      error("failed to compile "..v[1].."!")
    end
  end
end

-- * --------------------------------------------------------------------------

writeSeparator()

local iro_objs = List{}

do
  io.write("Building iro...\n")

  cd "iro"

  ---@type cmd.CppObj.Params | {}
  local cpp_params = {}

  cpp_params.compiler = "clang++"
  cpp_params.defines = List
  {
    {"IRO_CLANG", "1"},
  }
	
  if platform == "linux" then
	cpp_params.defines:push { "IRO_LINUX", "1" }
  else
	cpp_params.defines:push { "IRO_WIN32", "1" }
  end

  cpp_params.include_dirs = List
  {
    "../luajit/build/include",
  }
  cpp_params.opt = "none"
  cpp_params.debug_info = true

  local cpp_cmd = build_cmds.CppObj.new(cpp_params)

  ---@type cmd.LuaObj.Params | {}
  local lua_params = {}

  lua_params.debug_info = true

  local lua_cmd = build_cmds.LuaObj.new(lua_params)

  local c_to_o = List{}
  local l_to_o = List{}

  getSourceToObj("src", iro_build, c_to_o, l_to_o)

  walkDir("src", function(path)
    path = path:sub(#"src/" + 1)
    local dirname = path:match("(.*)/")
    if dirname then
      lfs.mkdir(iro_include.."/iro/"..dirname)
    end
    lfs.link(root.."/iro/src/"..path, iro_include.."/iro/"..path)
  end)

  compileObj(c_to_o, cpp_cmd)
  compileObj(l_to_o, lua_cmd)

  c_to_o:each(function(v) iro_objs:push(v[2]) end)
  l_to_o:each(function(v) iro_objs:push(v[2]) end)
end

-- * --------------------------------------------------------------------------

writeSeparator()

do 
  io.write("Building lake...\n")

  cd "../lake"

  ---@type cmd.CppObj.Params | {}
  local cpp_params = {}

  cpp_params.compiler = "clang++"
  cpp_params.defines = List
  {
    {"IRO_CLANG", "1"},
  }

  if platform == "linux" then
	cpp_params.defines:push { "IRO_LINUX", "1" }
  else
	cpp_params.defines:push { "IRO_WIN32", "1" }
  end

  cpp_params.include_dirs = List
  {
    iro_include,
    "../luajit/build/include",
  }
  cpp_params.opt = "none"
  cpp_params.debug_info = true

  local cpp_cmd = build_cmds.CppObj.new(cpp_params)

  ---@type cmd.LuaObj.Params | {}
  local lua_params = {}

  lua_params.debug_info = true

  local lua_cmd = build_cmds.LuaObj.new(lua_params)

  ---@type cmd.Exe.Params | {}
  local exe_params = {}

  exe_params.linker = "lld"
  exe_params.libdirs = List
  {
    "../luajit/build/lib",
  }
  exe_params.static_libs = List
  {
    "luajit",
    "lua51"
  }
  exe_params.shared_libs = List
  {
    "ws2_32"
  }
  exe_params.debug_info = true

  local exe_cmd = build_cmds.Exe.new(exe_params)

  local c_to_o = List{}
  local l_to_o = List{}

  getSourceToObj("src", lake_build, c_to_o, l_to_o)

  compileObj(c_to_o, cpp_cmd)
  compileObj(l_to_o, lua_cmd)

  local objs = List{}

  iro_objs:each(function(o) objs:push(o) end)
  l_to_o:each(function(v) objs:push(v[2]) end)
  c_to_o:each(function(v) objs:push(v[2]) end)

  local exe_name
  if platform == "linux" then
    exe_name = "lake"
  elseif platform == "windows" then
    exe_name = "lake.exe"
  end

  if 0 ~= execBuildCmd(exe_cmd:complete(objs, lake_build.."/"..exe_name))
  then
    error "failed to link lake!"
  end

  cd ".."

  copyFile(lake_build.."/"..exe_name, "bin/"..exe_name)

  if platform == "windows" then
    copyFile(lake_build.."/lake.pdb", "bin/lake.pdb")
  end
end

-- * --------------------------------------------------------------------------
do 
  io.write "Building elua...\n"

  cd "elua"

  ---@type cmd.CppObj.Params | {}
  local cpp_params = {}

  cpp_params.compiler = "clang++"
  cpp_params.defines = List
  {
    {"IRO_CLANG", "1"},
  }

  if platform == "linux" then
	cpp_params.defines:push { "IRO_LINUX", "1" }
  else
	cpp_params.defines:push { "IRO_WIN32", "1" }
  end

  cpp_params.include_dirs = List
  {
    iro_include,
    "../luajit/build/include",
  }
  cpp_params.opt = "none"
  cpp_params.debug_info = true

  local cpp_cmd = build_cmds.CppObj.new(cpp_params)

  ---@type cmd.Exe.Params | {}
  local exe_params = {}

  exe_params.linker = "lld"
  exe_params.libdirs = List
  {
    "../luajit/build/lib",
  }
  exe_params.static_libs = List
  {
    "luajit",
    "lua51"
  }
  exe_params.shared_libs = List
  {
    "ws2_32"
  }

  local exe_cmd = build_cmds.Exe.new(exe_params)

  local c_to_o = List{}

  local elua_build = build_dir.."/elua"
  lfs.mkdir(elua_build)

  getSourceToObj("src", elua_build, c_to_o)

  compileObj(c_to_o, cpp_cmd)

  local objs = List{}

  objs:pushList(iro_objs)
  c_to_o:each(function(v) objs:push(v[2]) end)

  local exe_name
  if platform == "windows" then
    exe_name = "elua.exe"
  else
    exe_name = "elua"
  end

  if 0 ~= execBuildCmd(exe_cmd:complete(objs, elua_build.."/"..exe_name))
  then
    error "failed to link elua!"
  end

  cd ".."

  copyFile(elua_build.."/"..exe_name, "bin/"..exe_name)
end
