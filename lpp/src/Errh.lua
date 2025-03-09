local lpp = require "Lpp"

return function(errmsg)
  if errmsg == lpp.cancel then return lpp.cancel end 

  local first_menv
  local stack = {}
  local fidx = 2
  while true do
    local info = debug.getinfo(fidx)
    if not info then break end
    if info.what ~= "C" and not lpp.err_func_filter[info.func] then 
      table.insert(stack, 1,
        {
          src = info.source,
          line = info.currentline,
          name = lpp.err_func_rename[info.func] or info.name,
          metaenv = getfenv(info.func).__metaenv,
        })
    end
    fidx = fidx + 1
  end

  for _, s in ipairs(stack) do
    if s.name then
      io.write(s.src, ":", s.line, ": in ", s.name, ":\n")
    else
      io.write(s.src, ":", s.line, ":\n")
    end
  end
  io.write(errmsg, "\n")

  return
  {
    stack = stack,
    msg = errmsg:gsub("%[.-%]:%d-: ", "")
                :gsub(".-:%d-: ", "")
  }
end
