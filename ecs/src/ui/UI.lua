local lpp = require "lpp"
local Schema = require "ui.Schema"

local ui = {}

ui.schemas = {}

-- * --------------------------------------------------------------------------

ui.getStyleContext = function(schemaname, varname)
  local schema = 
    assert(ui.schemas[schemaname], "no schema with name "..schemaname)

  return setmetatable({},
  {
    -- @style.<>
    __index = function(_, key)
      local property = 
        assert(schema:findMember(key), 
          "no property '"..key.."' in schema '"..schemaname.."'")
      local aliased_property = property.aliased_property

      local getLookup = function(typename)
        local name
        local defval
        if aliased_property then
          name = aliased_property.name
          defval = aliased_property.defval
        else
          name = property.name
          defval = property.defval
        end

        local lookup = 
          varname..".getAs<"..typename..">(\""..
          name.."\"_str, "..defval..")"

        if aliased_property then 
          lookup = property.defval:gsub("%%", lookup)
        end

        return lookup
      end

      return setmetatable({},
      {
        -- @style.<>.
        __index = function(_, key)
          -- Used on C type properties to access a member.
          local type = property.type
          assert(Schema.CType.isTypeOf(type), 
            "cannot index a property thats not a C type")

          return function()
            return getLookup(type.name).."."..key
          end
        end,
        -- Invoked when we don't try accessing a member of the style property.
        -- This could be a C type, enum, or flags.
        __call = function(_)
          local type = property.type

          if aliased_property then
            type = aliased_property.type
          end

          local switch =
          {
          [Schema.CType] = type.name,
          [Schema.Enum] = "u32",
          [Schema.Flags] = "u64",
          }

          local case = 
            assert(switch[getmetatable(type)], 
              "unhandled schema property type")

          return getLookup(case)
        end
      })
    end
  })
end

-- * --------------------------------------------------------------------------

ui.defineSchema = function(name, def)
  local file_offset = lpp.getMacroArgOffset(2)

  -- TODO(sushi) it would be nice to pcall here to prevent showing internal
  --             errors but they're also useful for debugging so idk.
  ui.schemas[name] = Schema.new(name, def, file_offset)
end

return ui
