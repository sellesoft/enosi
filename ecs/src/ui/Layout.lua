local cmn = require "common"
local Parser = require "Parser"

return function(var, text, offset)
  local layout = var.."->layout"
  local out = cmn.buffer.new()

  local function setVar(var, val)
    out:put(layout, ".", var, " = ", val, ";\n")
  end

  local function callMethod(var, method, ...)
    out:put(layout, ".", var, ".", method, "(", ..., ");\n")
  end

  local function qualify(name)
    return "ui::ItemLayout::"..name
  end

  local parser = Parser.new(text, offset)

  parser:skipWhitespace()

  -- Optionally start with a vertical bar, because it looks nice.
  parser:checkToken '|'

  local function checkInterpolation()
    if parser:checkToken "%$" then
      local exp = 
        parser:expectPattern("%b()", 
        "parenthesized expression for interpolation")
      return exp
    end
  end

  local function expectNumberOrInterp()
    local interp = checkInterpolation()
    if interp then
      return interp, true
    end
    local result = parser:checkNumber()
    if result then
      return result, false
    end
    parser:errorHere("expected a number or interpolation")
  end

  local function checkNumberOrInterp()
    local interp = checkInterpolation()
    if interp then
      return interp, true
    end
    local result = parser:checkNumber()
    if result then
      return result, false
    end 
    return nil
  end

  local function parsePos()
    local x, is_interp = expectNumberOrInterp()

    local y
    if is_interp then
      y = checkNumberOrInterp()
      if not y then
        -- A single interp argument is just directly setting the 
        -- var.
        setVar("bounds", x)
        return
      end
    else
      y = expectNumberOrInterp()
    end

    setVar("bounds.x", x)
    setVar("bounds.y", y)
  end

  local function parseSize()
    parser:skipWhitespace()

    local function parseFlex()
      parser:expectToken "%("
      local val = expectNumberOrInterp()
      parser:expectToken "%)"

      return val
    end

    local function parseComp()
      if parser:checkToken "flex" then
        return 
        {
          kind = "flex",
          val = parseFlex()
        }
      elseif parser:checkToken "auto" then
        return { kind = "auto" }
      else
        local c, is_interp = checkNumberOrInterp()
        if not c then 
          return 
        end
        local result = { kind = "basic", val = c, is_interp = is_interp }
        if parser:checkToken "%%" then
          result.kind = "percent"
        end
        return result
      end
    end

      -- callMethod("sizing", "set", qualify "Sizing::Flex")
      -- setVar("bounds."..comp, val)

    local start = parser.offset

    local x = parseComp()
    local y = parseComp()

    assert(x)

    if not y then
      if not x.is_interp then
        parser:errorAt(start, 
          "must provide both components for size when giving a single ",
          "interpolation argument")
      end
      if x.kind == "percent" then
        parser:errorAt(start, 
          "cannot use percents when providing a single interpolation argument")
      end
      callMethod("bounds", "setSize", x.val)
    else
      if x.kind == "flex" then
        callMethod("sizing", "set", qualify "Sizing::Flex")
      elseif x.kind == "percent" then
        callMethod("sizing", "set", "Sizing::PercentX")
      elseif x.kind == "auto" then
        callMethod("sizing", "set", "Sizing::AutoX")
      end
      setVar("bounds.w", x.val)

      if y.kind == "flex" then
        callMethod("sizing", "set", qualify "Sizing::Flex")
      elseif y.kind == "percent" then
        callMethod("sizing", "set", "Sizing::PercentY")
      elseif y.kind == "auto" then
        callMethod("sizing", "set", "Sizing::AutoY")
      end
      setVar("bounds.h", y.val)
    end
  end

  local function parseBoxBounds(name)
    return function()
      local x = checkNumberOrInterp()

      local y = checkNumberOrInterp()
      if y then
        local z = checkNumberOrInterp()
        if z then
          local w = checkNumberOrInterp()
          if w then
            setVar(name..".x", x)
            setVar(name..".y", y)
            setVar(name..".z", z)
            setVar(name..".w", w)
          else
            setVar(name..".x", y)
            setVar(name..".y", x)
            setVar(name..".z", y)
            setVar(name..".w", z)
          end
        else
          setVar(name..".x", y)
          setVar(name..".y", x)
          setVar(name..".z", y)
          setVar(name..".w", x)
        end
      else
        setVar(name..".x", x)
        setVar(name..".y", x)
        setVar(name..".z", x)
        setVar(name..".w", x)
      end
    end
  end

  local function checkEnumElem(def)
    local interp = checkInterpolation()
    if interp then 
      return true, interp
    end
    
    local id = parser:checkIdentifier()
    for elem in cmn.List(def):each() do
      if elem == id then
        return true, qualify(def.typename).."::"..elem
      end
    end

    return false, id
  end

  local function expectEnumElem(def)
    local success, result = checkEnumElem(def)
    if success then
      return result
    end
    parser:errorHere("no element '", result, "' in enum ", 
                     qualify(def.typename))
  end

  local function parseEnum(name, def)
    return function()
      setVar(name, expectEnumElem(def))
    end
  end

  local function parseFlags(name, def)
    return function()
      local elem = expectEnumElem(def)
      
      while true do
        local success, result = checkEnumElem(def)
        if not success then
          break
        end

        elem = elem.." | "..result
      end

      setVar(name, elem)
    end
  end

  local function parseProp()
    local name = parser:expectIdentifier()
    parser:expectToken ':'

    print(name)
      
    local props = 
    {
      pos     = parsePos,
      size    = parseSize,
      margin  = parseBoxBounds "margin",
      border  = parseBoxBounds "border",
      padding = parseBoxBounds "padding",

      positioning = parseEnum("positioning",
      {
        typename = "Positioning",
        "Static",
        "Relative",
        "Absolute",
        "Fixed",
      }),

      anchor = parseEnum("anchor",
      {
        typename = "Anchor",
        "TopLeft",
        "TopRight",
        "BottomRight",
        "BottomLeft",
      }),

      overflow = parseEnum("overflow",
      {
        typename = "Overflow",
        "Scroll",
        "Clip",
        "Visible",
      }),

      sizing = parseFlags("sizing",
      {
        typename = "Sizing",
        "Normal",
        "AutoX",
        "AutoY",
        "PercentX",
        "PercentY",
        "Square",
        "Flex",
      }),

      display = parseFlags("display",
      {
        typename = "Display",
        "Horizontal",
        "Flex",
        "Reverse",
        "Hidden",
      }),
    }

    local prop = props[name]
    if not prop then
      parser:errorHere("unknown property '", name, "'")
    end
    prop()
  end

  while true do 
    parseProp()

    if not parser:checkToken '|' then
      break
    end
  end

  io.write(out)

  return out:get()
end
