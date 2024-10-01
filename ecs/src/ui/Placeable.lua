--- 
--- A 'placeable' set of UI widgets and such.
---

local Type = require "Type"
local Parser = require "Parser"
local List = require "list"
local util = require "util"
local buffer = require "string.buffer"
local CGen = require "cgen"
local Style = require "ui.Style"

local WidgetNode,
      StyleClass,
      PropertyValue,
      PropertyInherit

-- * ==========================================================================
-- *   Placeable 
-- * ==========================================================================

---@class Placeable
--- The name of this Placeable. Used as its struct name.
---@field name string
--- The root, unused widget node.
---@field root WidgetNode
--- Reusable style settings.
---@field style_classes table
local Placeable = Type.make()

-- * --------------------------------------------------------------------------

Placeable.new = function(name, def, file_offset)
  local ui = require "ui.UI"
  local o = setmetatable({}, Placeable)
  o.name = name
  o.style_classes = {}
  o.root = WidgetNode.new(ui.widgets.Item)
  o:parse(def, file_offset)
  return o
end

-- * --------------------------------------------------------------------------

Placeable.parse = function(self, def, file_offset)
  local ui = require "ui.UI"
  local parser = Parser.new(def, file_offset)

  local parseDeclarations

  local parsePropertyList = function(widget, schema, parent, list)
    while true do 
      local classname = parser:checkPattern "%.[%w_]+"
      if classname then
        classname = classname:sub(2)
        
        local sc = parent:findStyleClass(classname)
        if not sc then
          parser:errorHere("no style class '",classname,"' in this scope")
        end
        
        sc.properties:each(function(prop)
          list:push(prop)
        end)
      else
        local propname = parser:expectIdentifier()

        local property = schema:findProperty(propname)
        if not property then
          parser:errorHere("no property named '",propname,"' in schema of ",
                           widget.name)
        end

        parser:expectToken ":"

        if parser:checkToken "inherit" then
          list:push(PropertyInherit.new(property))
        else
          local result
          if not property.valueParser then
            if not property.type.valueParser then
              parser:errorHere("property ", property.name, " does not define ",
                               "a value parser nor does its underlying type")
            end
            
            print(parser.text:sub(parser.offset, parser.offset + 10))

            result = property.type:valueParser(property, parser)
          else
            -- TODO(sushi) loading the lua chunks for parsing these values 
            --             should only be done once per property.
            local vparser, err = loadstring(
              "return function(parser, style)\n"..property.valueParser..
              "\nend",
              property.name..":valueParser()")

            if vparser == nil then
              parser:errorHere("failed to load valueParser for property "..
                               property.name, ":\n", err)
            end

            local style = 
            {
              set = setmetatable({},
              {
                __index = function(_, key)
                  local prop = schema:findProperty(key)
                  if not prop then
                    parser:errorHere("no property named ",key," in schema ",
                                     "of widget ",widget.name)
                  end
                  return function(val)
                    list:push(
                      PropertyValue.new(
                        prop,
                        prop.type:set(prop, val)))
                  end
                end,
              })
            }

            local res, err = pcall(vparser(), parser, style)

            if not res then
              error(err)
            end

            result = err
          end

          list:push(PropertyValue.new(property, result))
        end

      end
      if not parser:checkToken "|" then
        return
      end
    end
  end

  local parseWidgetPlacement = function(widget, node, parent)
    local schema = widget.schema_def

    local name = parser:checkIdentifier()
    if name then
      node.name = name
    end

    if parser:checkToken "|" then
      -- Parsing style properties.
      parsePropertyList(widget, schema, parent, node.properties)
    end

    if parser:checkToken "%(" then
      if widget.schema_def.makeParser then
        local mparser, err = loadstring(
          "return function(parser)\n"..widget.schema_def.makeParser.."\nend",
            widget.name..":makeParser()")

        if not mparser then
          parser:errorHere("failed to load makeParser for widget ",widget.name,
                           ":\n", err)
        end

        local res = {pcall(mparser(), parser)}

        if not res[1] then
          error(res[2])
        end
        
        -- really silly
        List{unpack(res, 2)}:each(function(arg)
          node.make_args:push(arg)
        end)

      else
        parser:errorHere("cannot define make arguments here as widget '",
                         widget.name,"' does not define a parser for them")
      end

      parser:expectToken "%)"
    end

    if parser:checkToken ";" then
      return
    elseif parser:checkToken "{" then
      if schema.terminal then
        parser:errorHere("schema of widget ",widget.name," defines it "..
                         "as terminal. Nested widgets cannot be used ",
                         "here.")
      end
      parseDeclarations(node)
    else
      parser:errorHere("expected ; or {")
    end
  end

  parseDeclarations = function(parent)
    while true do 
      local top_token

      top_token = parser:checkPattern "%.[%w_]+"
      if top_token then
        -- This is a style class definition.
        local name = top_token:sub(2)
        if parent.style_classes[name] then
          parser:errorHere("style class '",name,"' already exists in this ",
                           "scope")
        end
        
        local widget
        local schema
        if not parent then
          widget = ui.widgets.Item
          schema = ui.widgets.Item.schema_def
        else
          widget = parent.widget
          schema = parent.widget.schema_def
        end

        local sc = StyleClass.new(name)

        parser:expectToken "|"

        parsePropertyList(widget, schema, parent, sc.properties)

        parser:expectToken ";"

        parent.style_classes[name] = sc

        goto continue
      end

      top_token = parser:checkIdentifier()
      if top_token then
        -- This must be a widget name.
        local widget = ui.widgets[top_token]
        if not widget then
          parser:errorHere(top_token," does not name a UI widget")
        end
        local node = WidgetNode.new(widget, parent)
        parent.children:push(node)
        parseWidgetPlacement(widget, node, parent)
        goto continue
      end

      if parser:checkToken "}" then
        break
      else
        parser:errorHere("expected a widget identifier, style class, or }")
      end

      ::continue::
    end
  end

  parser:expectToken "{"

  -- TODO(sushi) make it so that StyleClasses dont rely on an existing schema
  --             and are just maps from property names to values then check
  --             when we apply them if they are valid so that we don't have to
  --             use an actual WidgetNode here.
  parseDeclarations(self.root)

  print(self:makeC())
end

-- * --------------------------------------------------------------------------

Placeable.makeC = function(self)
  local ui = require "ui.UI"
  local c = CGen.new()

  local formNodeName = function(node, name, anon_counts, prefix)
    -- Prefer name override, then the name provided for the node,
    -- then an autogenerated name.
    if not name then
      name = node.name
    end
    if not name then
      anon_counts[node.widget.name] = 
        anon_counts[node.widget.name] or 0
      name = node.widget.name:lower()..anon_counts[node.widget.name]
      anon_counts[node.widget.name] = 
        anon_counts[node.widget.name] + 1
    end
    return (prefix or "")..name
  end

  local placeWidgetRef = function(node, name, anon_counts)
    local name = formNodeName(node, name, anon_counts)
    c:appendStructMember(node.widget.name.."*", name, "nullptr")
  end

  local function placeChildStructDef(node, anon_counts)
    c:beginStruct()
    do
      local anon_counts = {}
      -- Place the widget ref representing this item.
      placeWidgetRef(node, "item", anon_counts)
      node.children:each(function(child)
        -- Place the widget representing this item.
        if child.children:len() > 0 then
          placeChildStructDef(child, anon_counts)
        else
          -- Terminal node so just place it directly.
          placeWidgetRef(child, nil, anon_counts)
        end
      end)
    end
    c:endStruct(formNodeName(node, nil, anon_counts))
  end

  c:beginStruct(self.name)
  do
    local anon_counts = {}
    self.root.children:each(function(child)
      placeChildStructDef(child, anon_counts)
    end)

    c:beginFunction("void", "place", "ui::UI& ui")
    do
      local formMakeArgs = function(node)
        local makeargs
        if node.make_args:len() > 0 then
          makeargs = ""
          node.make_args:each(function(arg)
            makeargs = makeargs..","..arg
          end)
        end
        return makeargs
      end

      ---@param node WidgetNode
      local setProperties = function(namepath, node)
        node.properties:each(function(prop)
          c:append(
            Style.setAs(
              namepath.."->style", 
              prop.property, 
              prop.value,
              namepath),";")
        end)
      end

      local makeWidget = function(namepath, node)
        local makeargs = formMakeArgs(node)
        c:append(
          namepath,' = ui.make<',node.widget.name,'>("', 
          self.name.."."..namepath,'"_str',makeargs or "",");")
        setProperties(namepath, node)
      end

      local function beginWidget(namepath, node)
        local anon_counts = {}
        local makeargs = formMakeArgs(node)
        c:append(
          namepath,'.item = ui.begin<',node.widget.name,'>("',
          self.name.."."..namepath,'"_str', makeargs or "", ");")
        c:beginScope()
        do
          setProperties(namepath..".item", node)
          node.children:each(function(child)
            local name = formNodeName(child, nil, anon_counts, namepath..".")
            if child.children:len() > 0 then
              beginWidget(name, child)
            else
              makeWidget(name, child)
            end
          end)
        end
        c:endScope()
        c:append("ui.end<",node.widget.name,">();")
      end

      self.root.children:each(function(child)
        local anon_counts = {}
        local name = formNodeName(child, nil, anon_counts)
        if child.children:len() > 0 then
          beginWidget(name, child)
        else
          makeWidget(name, child)
        end
      end)
    end
    c:endFunction()
  end
  c:endStruct()

  return c.buffer:get()
end

-- * ==========================================================================
-- *   WidgetNode
-- * ==========================================================================

--- A widget to be placed in hierarchical fashion.
---@class WidgetNode
--- Optional name for this widget placement.
---@field name string? 
--- The widget this node places.
---@field widget ui.Widget
--- Children of this node.
---@field children List
--- List of style properties to set for this widget.
---@field properties List
--- List of style classes defined in this widget's scope. Mostly for use 
--- internally.
---@field style_classes List
--- Arguments to be passed to this widget's make function.
---@field make_args List
WidgetNode = Type.make()

-- * --------------------------------------------------------------------------

WidgetNode.new = function(widget, parent)
  local o = {}
  o.name = false
  o.widget = widget
  o.parent = parent
  o.children = List{}
  o.properties = List{}
  o.style_classes = {}
  o.make_args = List{}
  return setmetatable(o, WidgetNode)
end

-- * --------------------------------------------------------------------------

WidgetNode.findStyleClass = function(self, name)
  local sc = self.style_classes[name] 
  if not sc and self.parent then
    return self.parent:findStyleClass(name)
  end
  return sc
end

-- * ==========================================================================
-- *   WidgetNode
-- * ==========================================================================

StyleClass = Type.make()

-- * --------------------------------------------------------------------------

StyleClass.new = function(name, prev)
  local o = {}
  o.properties = List{}
  return setmetatable(o, StyleClass)
end

-- * ==========================================================================
-- *   PropertyValue
-- * ==========================================================================

PropertyValue = Type.make()

-- * --------------------------------------------------------------------------

PropertyValue.new = function(property, value)
  local o = {}
  o.property = property
  o.value = value
  return setmetatable(o, PropertyValue)
end

-- * ==========================================================================
-- *   PropertyInherit
-- * ==========================================================================

PropertyInherit = Type.make()

-- * --------------------------------------------------------------------------

PropertyInherit.new = function(property)
  local o = {}
  o.property = property
  return setmetatable(o, PropertyInherit)
end


return Placeable
