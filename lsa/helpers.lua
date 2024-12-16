local helpers = {}

helpers.indexer = function(f)
  return setmetatable({}, { __index = f })
end

helpers.callindexer = function(fc, fi)
  return setmetatable({}, { __index = fi, __call = fc })
end

return helpers
