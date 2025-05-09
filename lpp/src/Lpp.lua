--
--
-- lpp api
--
--

local ffi = require "ffi"
local C = ffi.C
ffi.cdef 
[[
  String getTokenIndentation(void* lpp, s32);

  typedef struct MetaprogramBuffer { void* memhandle; s64 memsize; } MetaprogramBuffer;
  MetaprogramBuffer processFile(void* ctx, String path);
  void getMetaprogramResult(MetaprogramBuffer mpbuf, void* outbuf);

  typedef struct Metaprogram Metaprogram;
  typedef struct SectionNode SectionNode;
  typedef struct Scope Scope;

  void metaprogramAddMacroSection(
    Metaprogram* ctx, 
    u64 start, 
    u64 macro_idx);
  void metaprogramAddDocumentSection(Metaprogram* ctx, u64 start, String s);
  void metaprogramAddDocumentSpanSection(Metaprogram* mp, u64 start);
  void metaprogramAddMacroImmediateSection(Metaprogram* ctx, u64 start);

  String metaprogramGetMacroIndent(Metaprogram* ctx);

  SectionNode* metaprogramGetCurrentSection(Metaprogram* mp);
  SectionNode* metaprogramGetNextSection(Metaprogram* ctx);

  SectionNode* sectionNext(SectionNode* section);
  SectionNode* sectionPrev(SectionNode* section);

  b8 sectionIsMacro(SectionNode* section);
  b8 sectionIsDocument(SectionNode* section);

  b8 sectionInsertString(SectionNode* section, u64 offset, String s);
  b8 sectionAppendString(SectionNode* section, String s);

  String sectionGetString(SectionNode* section);

  b8 sectionConsumeFromBeginning(SectionNode* section, u64 len);

  String metaprogramGetOutputSoFar(Metaprogram* ctx);
  void metaprogramTrackExpansion(Metaprogram* ctx, u64 from, u64 to);
  s32 metaprogramMapMetaprogramLineToInputLine(Metaprogram* mp, s32 line);
  String metaprogramConsumeCurrentScopeString(Metaprogram* mp);

  SectionNode* metaprogramGetTopMacroInvocation(Metaprogram* mp);
  Metaprogram* metaprogramGetPrev(Metaprogram* mp);

  Scope* metaprogramGetCurrentScope(Metaprogram* mp);
  Scope* scopeGetPrev(Scope* scope);
  u64 scopeGetInvokingMacroIdx(Scope* scope);

  u64 sectionGetStartOffset(SectionNode* section);

  typedef struct
  {
    u64 line;
    u64 column;
  } LineAndColumn;

  LineAndColumn metaprogramMapInputOffsetToLineAndColumn(
    Metaprogram* mp, 
    u64 offset);
]]

local log = require"Logger"("lpp.lua", Verbosity.Notice)

log:info "initializing lpp lua module\n"

local buffer = require "string.buffer"

local List = require "List"

local strtype = ffi.typeof("String")
local strToLua = function(s)
  return ffi.string(s.s, s.len)
end
local luaToStr = function(s)
  return strtype(s, #s)
end

string.startsWith = function(self, s)
  return self:sub(1,#s) == s
end

local makeStruct = function()
  local o = {}
  o.__index = o
  return o
end

local MacroExpansion,
      MacroPart,
      Section

-- * ==========================================================================
-- *   lpp
-- * ==========================================================================

--- API for interacting with lpp during the execution of a metaprogram.
---
---@class lpp
--- Handle to the internal representation of lpp.
---@field handle userdata 
--- Handle to the currently executing metaprogram's 'context'.
---@field context userdata
--- Reference to the currently executing metaprogram's environment.
---@field metaenv table
--- List of callbacks to be executed in reverse order on each Document 
--- section as we reach them.
---@field doc_callbacks List
--- List of callbacks to be executed in reverse order on the final result.
---@field final_callbacks List
--- Dependencies, eg. anything the preprocessed file imports, includes, etc.
---@field dependencies List
--- Include dirs specified via -I on the command line.
---@field include_dirs List
--- If lpp is generating a dep file or not.
---@field generating_dep_file boolean
--- List of arguments passed on the command line that weren't consumed by lpp.
---@field argv table
local lpp = {}

lpp.dependencies = List{}
lpp.include_dirs = List{}
lpp.argv = List{}
-- Set true in lpp.cpp if we are.
lpp.generating_dep_file = false
lpp.stacktrace_func_filter = {}
lpp.stacktrace_func_rename = {}

-- * --------------------------------------------------------------------------

lpp.doc_callbacks = List{}
lpp.runDocumentSectionCallbacks = function()
  if lpp.doc_callbacks:isEmpty() then return end

  local section = lpp.getCurrentSection()
  lpp.doc_callbacks:each(function(f)
    f(section)
  end)
end

-- * --------------------------------------------------------------------------

--- Registers a callback to be invoked at the beginning of each Document
--- section. It will be invoked before any callbacks registered before it.
---
---@param f function
lpp.registerDocumentSectionCallback = function(f)
  lpp.doc_callbacks:push(f)
end

-- * --------------------------------------------------------------------------

lpp.final_callbacks = List{}
lpp.runFinalCallbacks = function()
  if lpp.final_callbacks:isEmpty() then return end

  local result = lpp.getOutputSoFar()
  lpp.final_callbacks:each(function(f)
    f(result)
  end)
end

-- * --------------------------------------------------------------------------

--- Registers a callback to be invoked after the entire file has been 
--- processed. The result will be passed as an immutable string.
--- It will be invoked before any callbacks registered before it.
---
-- TODO(sushi) make this editable if ever needed.
---
---@param f function
lpp.registerFinal = function(f)
  lpp.final_callbacks:push(f)
end

lpp.generateDepFile = function()
  local buffer = buffer.new()
  local depset = {}
  lpp.dependencies:each(function(dep)
    if not depset[dep] then
      buffer:put(dep, "\n")
      depset[dep] = true
    end
  end)
  return buffer:get()
end

-- * --------------------------------------------------------------------------

lpp.source_final_callbacks = List{}
lpp.runSourceFinalCallbacks = function()
  if lpp.source_final_callbacks:isEmpty() then return end

  for cb in lpp.source_final_callbacks:each() do
    cb()
  end
end

-- * --------------------------------------------------------------------------

lpp.registerSourceFinalCallback = function(f)
  lpp.source_final_callbacks:push(f)
end

-- * --------------------------------------------------------------------------

-- Patch lua's print with one that prefixes the message with the srcloc of the 
-- print as print is only meant to be used for debug purposes and losing where 
-- they are is quite easy.
local lua_print = print
print = function(first, ...)
  local info = debug.getinfo(2)
  local src = info.source
  local cwd = lua__getcwd()
  if src:find("^"..cwd) then
    src = src:sub(#cwd+2)
  end 
  local line = info.currentline
  if lpp.context then
    line = C.metaprogramMapMetaprogramLineToInputLine(lpp.context, line)
  end
  lua_print(src..":"..line..": "..tostring(first), ...)
end

-- * --------------------------------------------------------------------------

-- TODO(sushi) docs. Adding for use internally.
lpp.addDependency = function(path)
  lpp.dependencies:push(path)
end

-- * --------------------------------------------------------------------------

--- Adds an include dir. Mostly for use internally.
lpp.addIncludeDir = function(path)
  lpp.include_dirs:push(path)
end

lpp.addArgv = function(arg)
  lpp.argv:push(arg)
end

-- * --------------------------------------------------------------------------

--- Immediately ends all preprocessing quietly and prevents lpp from 
--- outputting the final output file.
lpp.cancel = function()
  error(lpp.cancel)
end

-- * --------------------------------------------------------------------------

--- Process the file at 'path' with lpp. 
---
--- Returns the final output of the evaluated file as a string.
---
---@param path string
---@return string
lpp.processFile = function(path)
  if #path == 0 then
    error("lpp.processFile passed an empty path")
  end
  if not lpp.getFileFullPathIfExists(path) then
    error("could not find '"..path.."'")
  end
  local result = lua__processFile(lpp.handle, path)
  if not result then
    lpp.cancel()
  end
  return result
end
lpp.stacktrace_func_filter[lpp.processFile] = true

lpp.getFileFullPathIfExists = function(path)
  return lua__getFileFullPathIfExists(path)
end

-- * --------------------------------------------------------------------------

local lua_require = require
require = function(path)
  local normpath = path:gsub('%.', '/')
  if lpp.generating_dep_file then
    for pattern in package.path:gmatch("[^;]+") do
      local fullpath = 
        lpp.getFileFullPathIfExists(
          pattern:gsub("?", normpath))
      if fullpath then
        lpp.dependencies:push(fullpath)
      end
    end
  end

  return lua_require(path)
end

-- * --------------------------------------------------------------------------

--- Wrapper around including in C/C++ that tracks what is included so we
--- can generate proper depedencies for lake. This just returns the 
--- contents of the file and is intended to be used as a macro.
---
---@param path string
lpp.include = function(path)
  lpp.dependencies:push(path)
end

-- * --------------------------------------------------------------------------

--- Retrieve the current section.
---
---@return Section?
lpp.getCurrentSection = function()
  local s = C.metaprogramGetCurrentSection(lpp.context)
  if s ~= nil then
    return Section.new(s)
  end
end

-- * --------------------------------------------------------------------------

--- Retrieve the section after the calling macro, or nil if none follows.
---
---@return Section?
lpp.getSectionAfterMacro = function()
  local s = C.metaprogramGetNextSection(lpp.context)
  if s ~= nil then
    return Section.new(s)
  end
end

-- * --------------------------------------------------------------------------

--- Get a string of the metaprogram's output so far.
---
---@return string
lpp.getOutputSoFar = function()
  return strToLua(C.metaprogramGetOutputSoFar(lpp.context))
end

-- * --------------------------------------------------------------------------

--- Consumes the current scope (macro invocation) and returns its result as a 
--- string.
---
-- TODO(sushi) better document what exactly this is doing once its more clear 
--             to me 
--
---@return string
lpp.consumeCurrentScope = function()
  return strToLua(C.metaprogramConsumeCurrentScopeString(lpp.context))
end

-- * --------------------------------------------------------------------------

--- Translate an offset into the current input file to a line and column.
---
---@param offset number
lpp.getLineAndColumnFromOffset = function(offset)
  local lac = C.metaprogramMapInputOffsetToLineAndColumn(lpp.context, offset)
  return tonumber(lac.line), tonumber(lac.column)
end

-- * --------------------------------------------------------------------------

--- Get the offset into the input file that the macro arg at 'idx' starts.
lpp.getMacroArgOffset = function(idx)
  return lpp.metaenv.current_macro_arg_offsets[idx]
end

-- * --------------------------------------------------------------------------

lpp.getInputName = function()
  return lua__getInputName(lpp.handle)
end

-- * --------------------------------------------------------------------------

lpp.getCurrentInputSourceName = function()
  return lua__getCurrentInputSourceName(lpp.handle)
end

-- * ==========================================================================
-- *   Section
-- * ==========================================================================

--- A part of the Document being processed. 
---
--- Currently, this represents either a Document section, or a Macro section.
---
---@class Section
--- Handle to the internal representation of a Section.
---@field handle userdata
Section = makeStruct()
lpp.Section = Section

-- * --------------------------------------------------------------------------

---@private
--
-- Construct a new Section.
--
---@param handle userdata
Section.new = function(handle)
  return setmetatable({handle=handle}, Section)
end

-- * --------------------------------------------------------------------------

--- Get the Section following this one, or nil if this is the last
--- Section of the Document.
---
---@return Section?
Section.getNextSection = function(self)
  local s = C.sectionNext(self.handle)
  if s ~= nil then
    return Section.new(s)
  end
end

-- * --------------------------------------------------------------------------

--- Get the Section preceeding this one, or nil if this is the first
--- Section of the Document.
---
---@return Section?
Section.getPrevSection = function(self)
  local s = C.sectionNext(self.handle)
  if s ~= nil then
    return Section.new(s)
  end
end

-- * --------------------------------------------------------------------------

--- Check if this Section is a Macro.
---
---@return boolean
Section.isMacro = function(self) return 0 ~= C.sectionIsMacro(self.handle) end

-- * --------------------------------------------------------------------------

--- Check if this Section is Document text.
---
---@return boolean
Section.isDocument = function(self) 
  return 0 ~= C.sectionIsDocument(self.handle) 
end

-- * --------------------------------------------------------------------------

--- Gets a lua string containing the contents of this Section.
--- 
--- Returns nil if the section is not a Document.
---
---@return string?
Section.getString = function(self)
  if not self:isDocument() then return nil end
  return strToLua(C.sectionGetString(self.handle))
end

-- * --------------------------------------------------------------------------

--- Insert text at the given offset in this section.
---
---@param offset number
---@param text string
---@return boolean
Section.insertString = function(self, offset, text)
  return 0 ~= C.sectionInsertString(self.handle, offset, luaToStr(text))
end

-- * --------------------------------------------------------------------------

--- Appens text at the end of the given section.
---
---@param text string
---@return boolean
Section.appendString = function(self, text)
  return 0 ~= C.sectionAppendString(self.handle, luaToStr(text))
end

-- * --------------------------------------------------------------------------

--- Consume some bytes from the beginning of this section.
---
---@param len number
---@return boolean
Section.consumeFromBeginning = function(self, len)
  return 0 ~= C.sectionConsumeFromBeginning(self.handle, len)
end

-- * --------------------------------------------------------------------------

--- Get the offset into the input file where this section starts.
---
---@return number
Section.getStartOffset = function(self)
  return tonumber(C.sectionGetStartOffset(self.handle))
end

-- * --------------------------------------------------------------------------

--- Get the offset into the input file where the macro arg at 'idx' starts.
---
---@param idx number
---@return number
Section.getStartOffsetOfMacroArg = function(self, idx)
  return tonumber(
    C.sectionGetStartOffsetOfMacroArg(lpp.context, self.handle, idx))
end

-- * ==========================================================================
-- *   MacroExpansion
-- * ==========================================================================

--- The result of a macro invocation. This contains a sequence
--- of either strings and/or MacroParts that form the buffer that 
--- will replace the macro.
---
--- Currently these only work when they are the returned value 
--- of a macro.
---
---@class MacroExpansion
--- A list of string or MacroPart that will be joined to form the 
---@field list List 
MacroExpansion = makeStruct()
lpp.MacroExpansion = MacroExpansion

-- * --------------------------------------------------------------------------

--- Check if 'x' is a MacroExpansion.
---
---@param x any
---@return boolean
MacroExpansion.isTypeOf = function(x)
  return getmetatable(x) == MacroExpansion
end

-- * --------------------------------------------------------------------------

--- Construct a new MacroExpansion.
---
--- An initial sequence of strings and/or MacroParts may be passed.
---
---@param ... string | MacroPart
---@return MacroExpansion
MacroExpansion.new = function(...)
  return setmetatable(
  {
    list = List{...}
  }, MacroExpansion)
end

-- * --------------------------------------------------------------------------

--- Push a string or MacroPart to the front of this expansion.
---
---@param v string | MacroPart
---@return self
MacroExpansion.pushFront = function(self, v)
  self.list:pushFront(v)
  return self
end

-- * --------------------------------------------------------------------------

--- Push a string or MacroPart to the back of this expansion.
---
---@param v string | MacroPart
---@return self
MacroExpansion.pushBack = function(self, v)
  self.list:push(v)
  return self
end

-- * --------------------------------------------------------------------------

---@private
-- Call metamethod used internally to easily finalize a macro expansion.
-- When a macro returns a MacroExpansion it is immediately called.
--
---@param offset number The offset into the buffer this macro was used in.
MacroExpansion.__call = function(self, offset)
  local out = buffer.new()

  self.list:each(function(x)
    if MacroPart.isTypeOf(x)
    then
      if offset then
        C.metaprogramTrackExpansion(lpp.context, x.start, offset + #out)
      end
      out:put(tostring(x))
    else
      out:put(x)
    end
  end)

  return out:get()
end

-- * --------------------------------------------------------------------------

MacroExpansion.__tostring = function(self)
  -- Don't provide an offset to prevent tracking expansion
  -- as this is called when this is being turned into a string in lua code.
  return self()
end

-- * --------------------------------------------------------------------------

--- Override the concat operator to make MacroExpansion sorta able to 
--- behave like a normal string, in the same way MacroPart does. 
---
--- MacroExpansion can concat with a string, MacroPart, or another 
--- MacroExpansion. 
---
-- Note that MacroPart will never appear as 'lhs' here, because it 
-- defines its own __concat operator.
--
-- Also, lots of silly @casts here. Gonna keep it for now as it silences
-- luals's huge amount of warnings here, and I think its neat. Would be 
-- a lot nicer if it were capable of inferring type correctly from my
-- isTypeOf functions, though.
---
---@param lhs MacroExpansion | string
---@param rhs MacroExpansion | MacroPart | string
---@return MacroExpansion
MacroExpansion.__concat = function(lhs, rhs)
  local tryToString = function(from, to)
    assert(getmetatable(from),
      "attempt to concatenate a plain table with a MacroExpansion!")
    assert(getmetatable(from).__tostring,
      "attempt to concatenate a table with no '__tostring' method with a "..
      "MacroExpansion!")

    return to:pushBack(tostring(from))
  end

  if MacroExpansion.isTypeOf(lhs)
  then ---@cast lhs -string
    if MacroPart.isTypeOf(rhs) or
       "string" == type(rhs)
    then ---@cast rhs -MacroExpansion
      return lhs:pushBack(rhs)
    elseif MacroExpansion.isTypeOf(rhs)
    then ---@cast rhs MacroExpansion
      rhs.list:each(function(x)
        lhs:pushBack(x)
      end)
      return lhs
    else
      return tryToString(rhs, lhs)
    end
  else ---@cast lhs -MacroExpansion
     ---@cast rhs MacroExpansion
    if "string" == type(lhs)
    then
      return rhs:pushFront(lhs)
    else
      return tryToString(lhs, rhs)
    end
  end
end

-- * ==========================================================================
-- *   MacroPart
-- * ==========================================================================

--- A part of a MacroExpansion that contains the text to expand and information 
--- that can be used to determine where that text comes from.
---
--- Originally these were implicitly wrapping arguments passed to macros, 
--- but that didn't work as well as I wanted. We're not able to fully make them
--- masquarade as normal strings, primarily in the case of trying to use 
--- a macro argument to index a table. I would like to return to this as its
--- very useful for mapping from the output to macro arguments properly 
--- but currently I don't use those mappings for anything. Once we get to 
--- translating compiler errors and fixing debug info we should revisit this
--- in some way that isn't implicit.
---
--- OK these are now being used again but only to report where macro arguments
--- start to the metaenvironment so that the user can get this information.
--- For example, ECS uses this in ui schema parsing to give proper error
--- locations.
---
---@class MacroPart
--- A string naming where this part comes from. This may be any name, but 
--- ideally it is a path relative to the file the macro is being invoked in.
-- TODO(sushi) its very wasteful to make an entire string for each macro 
--             part when they will all be the same. Once we get back to 
--             using these for their original purpose add a way to get this
--             name to the lpp or section api instead.
---@field source string 
--- The byte offset into 'source' where this part begins.
---@field start number
--- The byte offset into 'source' where this part ends.
---@field stop number
--- The text this part represents.
---@field text string
MacroPart = makeStruct()
lpp.MacroPart = MacroPart

-- * --------------------------------------------------------------------------

--- Check if 'x' is a MacroPart
---
---@param x any
---@return boolean
MacroPart.isTypeOf = function(x)
  return getmetatable(x) == MacroPart
end

-- * --------------------------------------------------------------------------

--- Construct a new MacroPart
---
---@param source string
---@param start number
---@param stop number
---@param text string
---@return MacroPart
MacroPart.new = function(source, start, stop, text)
  return setmetatable(
  {
    source=source,
    start=start,
    stop=stop,
    text=text,
  }, MacroPart)
end

-- * --------------------------------------------------------------------------

--- Let MacroParts coerce to strings
MacroPart.__tostring = function(self)
  return self.text
end

-- * --------------------------------------------------------------------------

--- Consume a MacroPart and a string/MacroPart into a MacroExpansion or 
--- give this MacroPart to a MacroExpansion.
---
---@param lhs MacroPart | string
---@param rhs MacroPart | MacroExpansion | string
---@return MacroExpansion
MacroPart.__concat = function(lhs, rhs)
  if MacroExpansion.isTypeOf(rhs)
  then return MacroExpansion.__concat(lhs, rhs)
  else return MacroExpansion.__concat(lhs, MacroExpansion.new(rhs))
  end
end

-- so fucking desparate rn
lpp.debugBreak = function()
  lua__debugBreak()
end

return lpp
