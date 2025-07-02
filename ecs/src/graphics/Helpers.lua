-- 
-- Various stuff common to graphics files.
--

local cmn = require "common"

local m = {}

m.defCreateErr = function(type_name, var_name)
  return function(...)
    return 
      [[ERROR("while creating ]]..type_name..[[ '", ]]..var_name..
      [[, "': ",]]..cmn.joinArgs(',', ...)..[[, '\n');]]
  end
end

return m
