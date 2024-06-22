local outpath = arg[1].."lake.lua"

local lakepath = "src/lake.lua"

local f = io.open(lakepath, "r")

if not f then
  error("failed to open '"..lakepath.."' for reading")
end

local buffer = f:read("*a")

buffer = buffer:gsub("--%s*@include%s*\"(.-)\".*$", function(path)
  local include = io.open(path, "r")

  if not include then
    error("failed to open '"..path.."' for including")
  end

  return " = [[\n"..include:read("*a").."\n]]\n"
end)

local fo = io.open(outpath, "w")

if not fo then
  error("failed to open '"..outpath.."' for writing")
end

fo:write(buffer)
