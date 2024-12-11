local sys = require "build.sys"
local Project = require "build.project"
local driver = require "build.driver"
local helpers = require "build.helpers"

local lpp = Project.new("lpp", lake.cwd())

lpp:dependsOn "iro"

for cfile in lake.find("src/**/*.cpp"):each() do
  local ofile = cfile..".o"
end

helpers.createCppToObj(lpp, "src")




