--- Here, we build lake based on what platform we are running on.

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

  io.write("\n$ ", cmd, "\n\n")

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
   "gcc -shared src/lfs.c ",
   "-o ../lfs.so ",
   "-I../../luajit/include") then
  error "failed to compile lua filesystem!"
end

package.cpath = package.cpath..";./tmp/?.so"

local lfs = require "lfs"

local cd = function(path)
  io.write("\n--> cd ", path, "\n\n")
  lfs.chdir(path)
end

local build_dir = lfs.currentdir().."/tmp/build"
lfs.mkdir(build_dir)

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

local function getSourceToObj(path, bdir, c_to_o, l_to_o)
  walkDir(path, function(path)
    if path:match("%.cpp$") then
      table.insert(c_to_o, {path, bdir.."/"..path..".o"})
    elseif l_to_o and path:match("%.lua$") then
      table.insert(l_to_o, {path, bdir.."/"..path..".o"})
    end
    lfs.mkdir(bdir.."/"..path:match("(.*)/"))
  end)
end

local function compileCppObj(c_to_o)
  -- TODO(sushi) once the new build system is implemented use whatever its 
  --             equivalent to Driver is to generate this
  local compiler_flags = 
    "-std=c++20 "..
    "-I../iro "..
    "-I../luajit/include "..
    "-DIRO_LINUX "..
    "-DIRO_GCC "..
    "-O2 "

  for _,v in ipairs(c_to_o) do
    if 0 ~= exec("clang++ -c ",v[1]," -o ",v[2]," ",compiler_flags) then
      error("failed to compile "..v[1].."!")
    end
  end
end

local function compileLuaObj(l_to_o)
  for _,v in ipairs(l_to_o) do
    if 0 ~= exec("luajit -b -g ",v[1]," ",v[2]) then
      error("failed to compile "..v[1].."!")
    end
  end
end

writeSeparator()

io.write("Building iro...\n")

local iro_build = build_dir.."/iro"
lfs.mkdir(iro_build)

cd "iro"

local iro_c_to_o = {}
local iro_l_to_o = {}

getSourceToObj("iro", iro_build, iro_c_to_o, iro_l_to_o)

compileCppObj(iro_c_to_o)
compileLuaObj(iro_l_to_o)

writeSeparator()

local lake_build = build_dir.."/lake"
lfs.mkdir(lake_build)

io.write("Building lake...\n")

cd "../lake"

local lake_c_to_o = {}
local lake_l_to_o = {}

getSourceToObj("src", lake_build, lake_c_to_o, lake_l_to_o)

compileCppObj(lake_c_to_o)
compileLuaObj(lake_l_to_o)

-- TODO(sushi) get these earlier
local objs = ""

local function collectObjs(...)
  for _,x in ipairs{...} do
    for _,v in ipairs(x) do
      objs = objs..v[2].." "
    end
  end
end

collectObjs(lake_c_to_o, lake_l_to_o, iro_c_to_o, iro_l_to_o)

local linker_flags = 
  "-L../luajit/lib "..
  "-l:libluajit.a "..
  "-lexplain "..
  "-lm "..
  "-Wl,-E "

if 0 ~= exec("clang++ ",objs," ",linker_flags," -o ",lake_build.."/".."lake") 
then
  error "failed to link lake!"
end

cd ".."

if 0 ~= exec("cp ",lake_build,"/lake bin/lake") then
  error "failed to copy lake to bin/!"
end

io.write "Building elua...\n"

cd "elua"

local elua_c_to_o = {}

local elua_build = build_dir.."/elua"
lfs.mkdir(elua_build)

getSourceToObj("src", elua_build, elua_c_to_o)

compileCppObj(elua_c_to_o)

objs = ""

collectObjs(elua_c_to_o, iro_c_to_o, iro_l_to_o)

if 0 ~= exec("clang++ ",objs," ",linker_flags," -o ",elua_build,"/","elua")
then
  error "failed to link elua!"
end

cd ".."

if 0 ~= exec("cp ",elua_build,"/elua bin/elua") then
  error "failed to copy elua to bin/!"
end
