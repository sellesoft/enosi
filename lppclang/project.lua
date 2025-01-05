local sys = require "build.sys"
local bobj = require "build.object"
local List = require "list"

local lppclang = sys.getLoadingProject()

lppclang:dependsOn("llvm", "iro")

for cfile in lake.find("src/**/*.cpp"):each() do 
  lppclang.report.CppObj(cfile)
end

for lfile in lake.find("src/**/*.lua"):each() do
  lppclang.report.pub.LuaObj(lfile)
end

-- Get the clang executable as reported by the llvm project and define 
-- a path define to it for lppclang, as we need to use a proper path to the 
-- locally built clang due to how clang looks for certain files internally
-- (eg. stdint.h and stuff like that I think idk its been awhile).
local llvm = sys.getOrLoadProject "llvm"
local llvm_exes = List{}
llvm:getPublicBuildObjects(bobj.Exe, llvm_exes)

lppclang.report.defines 
{
  ENOSI_CLANG_EXECUTABLE='"'..llvm.root.."/"..llvm_exes[1].name..'"'
}

lppclang.report.published(
  lppclang.report.pub.SharedLib("lppclang",
    lppclang:gatherBuildObjects{bobj.CppObj, bobj.LuaObj}))
