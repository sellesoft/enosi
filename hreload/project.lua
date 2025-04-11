local sys = require "build.sys"
local List = require "List"
local bobj = require "build.object"

local hreload = sys.getLoadingProject()

hreload:dependsOn "iro"

local iro = sys.getOrLoadProject "iro"

hreload.report.dir.include 
{
  from = "src",
  to = "hreload",
  glob = "**/*.h",
}

for cfile in lake.find("src/**/*.cpp"):each() do
  hreload.report.CppObj(cfile)
end

-- Report and collect test exe files.
local testobjs = List{}
for cfile in lake.find "tests/**/*.cpp" :each() do
  testobjs:push(hreload.report.CppObj(cfile))
end

testobjs:pushList(iro:gatherBuildObjects{bobj.CppObj})

local sofilter = 
{
  -- Filter out luajit since we don't use it and it's not compiled 
  -- with -fPIC
  luajit = true,
}

if sys.patch then
  -- Some more dumb hacks to avoid issues with the build system. I really 
  -- need to work on it again.
  -- If we don't do this, the build system assumes that we need to link 
  -- the patched so into hreload's so, which causes a dependency cycle.
  sofilter["hreload-test.patch"..sys.patch] = true
end

hreload.report.pub.SharedLib("hreload", 
  hreload:gatherBuildObjects{bobj.CppObj}, 
  sofilter)

hreload.report.dir.lib
{
  direct = { hreload:getBuildDir() }
}

if sys.patch then
  hreload.report.SharedLib("hreload-test.patch"..sys.patch,
    hreload:gatherBuildObjects{bobj.CppObj},
    {
      luajit = true
    })

  -- Output a .hrf file for the test exe. Ideally this is integrated into
  -- the build system later on.
  local hrf = io.open(hreload:getBuildDir().."/hreload-test.hrf", "w")
  if not hrf then
    error("failed to open hrf file for writing")
  end

  for ofile in hreload:gatherBuildObjects{bobj.CppObj}:each() do
    if not ofile:getTargetName():find "Reloader" then
      if ofile.proj == hreload or ofile.proj == iro then
        hrf:write(
          "+o", ofile.proj:getBuildDir(), "/", ofile:getTargetName(), "\n")
      end
    end
  end
else
  hreload.report.Exe("hreload-test", testobjs)
end

