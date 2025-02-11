--- 
--- Functions for display things in a consistent and pretty way.
---

local sys = require "build.sys"
local helpers = require "build.helpers"

-- * --------------------------------------------------------------------------

local flair = {}

-- * --------------------------------------------------------------------------

flair.reset = "\027[0m"
flair.green = "\027[0;32m"
flair.blue  = "\027[0;34m"
flair.red   = "\027[0;31m"

-- * --------------------------------------------------------------------------

local rootRelativePath = function(path)
  return helpers.makeRootRelativePath(sys.root, path)
end

-- * --------------------------------------------------------------------------

flair.writeSuccessOnlyOutput = function(output, time)
  io.write(flair.blue, rootRelativePath(output), flair.reset)
  if time then
    io.write(" ", time, "s")
  end
  io.write "\n"
end

-- * --------------------------------------------------------------------------

flair.writeSuccessInputToOutput = function(input, output, time)
  io.write(
    flair.green, rootRelativePath(input), flair.reset, 
    " -> ", 
    flair.blue, rootRelativePath(output), flair.reset)
  if time then
    io.write(" ", time, "s")
  end
  io.write "\n"
end

-- * --------------------------------------------------------------------------

flair.writeFailure = function(output)
  io.write(flair.red, "failed to build ", flair.blue, 
           rootRelativePath(output), flair.reset, "\n")
end

return flair
