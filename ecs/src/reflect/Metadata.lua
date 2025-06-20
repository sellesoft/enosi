---
--- Module for attaching metadata via comments to C++ declarations.
--- Clang keeps track of comments on decls and we use this to attach
--- information to C++ things without needing to perform complex parsing
--- ourselves.
---

local M = {}
local metadata = setmetatable({}, M)

M.__index = function(_,k)
  return function(v)
    if v then
      v = v:gsub("[%c\"]",
        {
          ["\n"] = "\\n",
          ["\r"] = "",
          ['"'] = '\\"',
        })
    else
      v = true
    end
    if v then
      return "// .metadata "..k.." = \""..tostring(v).."\""
    end
  end
end

metadata.__parse = function(x)
  local o = {}
  for k,v in x:gmatch '%.metadata%s+([%w_]+)%s+=%s+([^\r\n]+)' do
    o[k] = v:sub(2,-2):gsub("\\[rn]", {["\\n"]="\n", ["\\r"]=""})
  end
  return o
end

return metadata
