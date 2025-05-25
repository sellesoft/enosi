local lpp = require "Lpp"

return function(errmsg)
  if errmsg == lpp.cancel then return lpp.cancel end 

  local result = {pcall(function()

    local stack = {}
    local fidx = 2
    while true do
      local info = debug.getinfo(fidx)
      if not info then break end
      if info.what ~= "C" and not lpp.stacktrace_func_filter[info.func] then 
        table.insert(stack, 1,
          {
            src = info.source,
            line = info.currentline,
            name = lpp.stacktrace_func_rename[info.func] or info.name,
            metaenv = getfenv(info.func).__metaenv,
          })
      end
      fidx = fidx + 1
    end

    return 
    {
      stack = stack,
      msg = errmsg:gsub("%[.-%]:%d-: ", "")
                  :gsub(".-:%d-: ", "")
    }
  end)}

  if result[1] then
    return result[2]
  end

  print(result[2])
    
end
