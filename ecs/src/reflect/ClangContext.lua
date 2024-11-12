---
--- Helper module for creating clang contexts. This handles properly passing
--- this translation units cli args to the created context, so use it!
---

local lpp = require "lpp"
local List = require "list"

-- Get the cargs so we can pass them to lppclang when we create the context.
local args = List{}

local nix_cflags = os.getenv("NIX_CFLAGS_COMPILE")
if nix_cflags then
  for arg in nix_cflags:gmatch("%S+") do
    args:push(arg)
  end
end

-- TODO(sushi) put this somewhere central lol this is dumb
string.startsWith = function(self, s)
  return self:sub(1, #s) == s
end

for _,v in ipairs(lpp.argv) do
  if v:startsWith "--cargs" then
    local cargs = v:sub(#"--cargs="+1)
    for carg in cargs:gmatch("[^,]+") do
      args:push(carg)
    end
  end
end

return function()
  return lpp.clang.createContext(args)
end
