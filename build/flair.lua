--- 
--- Functions for display things in a consistent and pretty way.
---

local sys = require "build.sys"
local helpers = require "build.helpers"

-- * --------------------------------------------------------------------------

local flair = {}

-- * --------------------------------------------------------------------------

local reset = "\027[0m"
local green = "\027[0;32m"
local blue  = "\027[0;34m"
local red   = "\027[0;31m"

-- * --------------------------------------------------------------------------

local rootRelativePath = function(path)
  return helpers.makeRootRelativePath(sys.root, path)
end

-- * --------------------------------------------------------------------------

flair.writeSuccessOnlyOutput = function(output, time)
  io.write(blue, rootRelativePath(output), reset)
  if time then
    io.write(" ", time, "s")
  end
  io.write "\n"
end

-- * --------------------------------------------------------------------------

flair.writeSuccessInputToOutput = function(input, output, time)
  io.write(
    green, rootRelativePath(input), reset, 
    " -> ", 
    blue, rootRelativePath(output), reset)
  if time then
    io.write(" ", time, "s")
  end
  io.write "\n"
end

-- * --------------------------------------------------------------------------

flair.writeFailure = function(output)
  io.write(red, "failed to build ", blue, rootRelativePath(output), reset, 
           "\n")
end

return flair
