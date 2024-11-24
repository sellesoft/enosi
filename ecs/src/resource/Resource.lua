--- NOTE(sushi) not in use atm, but should be persued when we have a better 
---             idea of how the asset management will work.

local Type = require "Type"

local Asset = {}

Asset.kinds = {}

Asset.define = function(name)

end

--- Helper for defining a 
local Ref = Type.make()
Asset.Ref = Ref

--- Information about a 'kind' of asset, eg. a Texture or Font.
local Kind = Type.make()
Asset.Kind = Kind

return Asset
