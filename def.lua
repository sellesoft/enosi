local glob = require "Glob"

glob "ecs/assets/**/*.png" :each(function(path)
  local name = path:match("ecs/(.+)%.png")

  local text = [[
return gfx::TextureDef
{
  name = "]]..name..[[.png",
  kind = "TwoD",
  filter = "Nearest",
  address_mode = "ClampToWhite",
}
  ]]
  
  local file = io.open("ecs/"..name..".texture", "w")
  if not file then
    error("failed to open "..name)
  end

  file:write(text)
  file:close()
end)
