--
--
-- Metaenvironment construction function.
--
-- This is used to create an environment table that the metaprogram 
-- runs in.
--
--

local ffi = require "ffi"
local C = ffi.C

local log = require "Logger" ("metaenv.lua", Verbosity.Info)
local stackcapture = require "StackCapture"
local List = require "List"
local buffer = require "string.buffer"

local strtype = ffi.typeof("String")
local makeStr = function(s)
  return strtype(s, #s)
end

return function(ctx)
  -- The metaenvironment table.
  -- All things in this table are accessible as 
  -- global things from within the metaprogram.
  local M = {}
  M.__index = _G

  setmetatable(M, M)

  M.__metaenv = {}
  local menv = M.__metaenv

  local lpp = require "Lpp"
  menv.lpp = lpp

  menv.prev = lpp.metaenv
  menv.ctx = ctx
  menv.macro_invokers = {}
  menv.macro_invocations = {}
  menv.current_macro_arg_offsets = nil
  menv.macro_names = {}

  menv.exited = false

  local assertNotExited = function(fname)
    if not menv.exited then return end
    -- TODO(sushi) show proper error locations here or add support for this :)
    log:fatal(debug.traceback(), "\n\n")
    log:fatal(
      "__metaenv.", fname, " called for a metaenvironment that has already ",
      "exited!\nThis may have occured because you managed to call a \n",
      "function defined in one lpp file from a different lpp file. This is \n",
      "currently not supported.\n")
    os.exit(1)
  end

  menv.doc = function(start, str)
    -- str could be nil in the case of immediate macro invocations that don't
    -- return anything.
    assertNotExited "doc"
    C.metaprogramAddDocumentSection(ctx, start, makeStr(str or ""))
  end

  menv.val = function(start, x)
    assertNotExited "val"
    if x == nil then
      error("inline lua expression resulted in nil", 2)
    end
    local s = tostring(x)
    if not s then
      error(
        "result of inline lua expression is not convertible to a string", 2)
    end
    C.metaprogramAddDocumentSection(ctx, start, makeStr(s))
  end
  lpp.stacktrace_func_filter[menv.val] = true

  menv.macro = function(start, indent, name, is_method, macro, ...)
    assertNotExited "macro"
    -- capture where this macro was invoked in phase 1
    local invoke_capture, pos = stackcapture(1)

    local invocation = debug.getinfo(2)
    table.insert(menv.macro_invocations, invoke_capture)

    if not macro then
      error("attempt to call a nil value as macro '"..name.."'")
    end

    local args = List{...}

    -- Extract the location information from the passed MacroParts and
    -- replace each arg with its actual string.
    -- This probably sucks performance-wise but whatever.
    local arg_offsets = {}
    args = args:map(function(arg)
      -- Skip the first arg on methods bc they will not be a macro part.
      if not is_method then
        table.insert(arg_offsets, arg.start)
        return arg.text
      else
        is_method = false
        return arg
      end
    end)

    invoker = function()
      local prev_macro_arg_offsets = menv.current_macro_arg_offsets
      menv.current_macro_arg_offsets = arg_offsets

      lpp.stacktrace_func_filter[invoker] = true

      local result = {xpcall(macro, function(err)
        return
        {
          stack = stackcapture(1),
          msg = err:gsub("%[.-%]:%d-: ", "")
                   :gsub(".-:%d-: ", ""),
        }
      end, unpack(args))}

      menv.current_macro_arg_offsets = prev_macro_arg_offsets

      if not result[1] then
        error(result[2])
      end

      result = result[2]

      if lpp.MacroExpansion.isTypeOf(result) then
        -- call the expansion table so that it reports how each part 
        -- expands
        return result()
      elseif "string" == type(result) or result == nil then
        return result
      else
        error "macro returned non-string value"
      end
    end

    table.insert(menv.macro_invokers, invoker)
    table.insert(menv.macro_names, name)
    C.metaprogramAddMacroSection(
      ctx, 
      makeStr(indent), 
      start, 
      #menv.macro_invokers)
  end

  menv.macro_immediate = function(start, name, is_method, macro, ...)
    if not macro then
      error("attempt to call a nil value as macro '"..name.."'")
    end

    local args = List{...}
    local arg_offsets = {}
    args = args:map(function(arg)
      -- Skip the first arg on methods bc they will not be a macro part.
      if not is_method then
        table.insert(arg_offsets, arg.start)
        return arg.text
      else
        is_method = false
        return arg
      end
    end)

    lpp.stacktrace_func_rename[macro] = name

    local prev_macro_arg_offsets = menv.current_macro_arg_offsets
    menv.current_macro_arg_offsets = arg_offsets

    C.metaprogramAddMacroImmediateSection(menv.ctx, start)

    local result = macro(unpack(args))

    menv.current_macro_arg_offsets = prev_macro_arg_offsets

    if lpp.MacroExpansion.isTypeOf(result) then
      return result()
    elseif "string" == type(result) or nil == result then
      return result
    else
      error "macro returned non-string value"
    end
  end
  lpp.stacktrace_func_filter[menv.macro_immediate] = true
  
  return M
end
