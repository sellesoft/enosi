--- 
--- Error handler used in lake.
---

return function(obj)
  print(debug.traceback())
  return obj
end
