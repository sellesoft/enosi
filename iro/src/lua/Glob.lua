---
--- Lua interface for iro's globber.
---

local List = require "List"

local ffi = require "ffi"
ffi.cdef
[[
  typedef struct
  {
    String* paths;
    s32 path_count;
  } GlobResult;

  GlobResult iro_glob(String);

  void iro_globFree(GlobResult);
]]
local C = ffi.C
local strtype = ffi.typeof("String")

---@param s string
---@return str
local make_str = function(s)
  if not s then
    error("make_str passed a nil s")
  end
  return strtype(s, #s)
end

return function(pattern)
  if not pattern then
    error("glob passed a nil pattern")
  end
  local glob = C.iro_glob(make_str(pattern))
  if glob.paths == 0 then
    return List{}
  end

  local out = List()

  for i=0,tonumber(glob.path_count-1) do
    local s = glob.paths[i]
    out:push(ffi.string(s.s,s.len))
  end

  C.iro_globFree(glob)

  return out
end
