local LuaType = require "Type"
local util = require "util"
local helpers = require "helpers"
local cmn = require "common"

--- Some characters of some kind as well as their location.
---@class lsa.Token
--- The kind of this token.
---@field kind lsa.Token.Kind
--- Offset into the input where this token starts.
---@field loc number
--- The length of this token.
---@field len number
local Token = LuaType.make()

Token.new = function(kind, start, stop)
  local o = {}
  o.kind = kind 
  o.loc = start
  o.len = stop-start+1
  return setmetatable(o, Token)
end

---@class lsa.Token.Kind
Token.Kind = {}

Token.Kind.__tostring = function(self)
  return self.name
end

Token.__tostring = function(self)
  return "Token("..tostring(self.kind)..")"
end

Token.str_to_kind = {}
Token.max_str_len = 0

Token.Kind.new = function(name, str, group, tag)
  local o = {}
  o.name = name

  if #group ~= 0 then o["is_"..group] = true end 
  if #tag ~= 0 then o["is_"..tag] = true end

  local str_len = #str
  if str_len ~= 0 then
    o.str = str
    if str_len > Token.max_str_len then
      Token.max_str_len = str_len
    end
    Token.str_to_kind[str] = o
  end

  Token.Kind[name] = o

  return setmetatable(o, Token.Kind)
end

local kinds = setmetatable(
{
  tag = "",
  group = "",
},
{
  __index = function(self,k)
    local kws = 
    {
      with_tag = helpers.indexer(function(_,k)
        self.tag = k
        return self
      end),

      tagEnd = function()
        self.tag = ""
        return self
      end,

      in_group = helpers.indexer(function(_,k)
        self.group = k
        return self
      end),

      groupEnd = function()
        self.group = ""
        return self
      end
    }

    local kw = kws[k]
    if kw then
      return kw
    end

    if Token.Kind[k] then
      fatal("a token kind '",k,"' is already defined")
    end

    return function(str)
      Token.Kind.new(k, str, self.group, self.tag)
      return self
    end
  end
})

kinds
  .Invalid ""
  .Eof ""
  .Whitespace ""
  .Comment ""
  .Identifier ""
  .with_tag.literal
    .Number ""
    .String ""
    .tagEnd()
  .in_group.punc
    .Colon ":"
    .Semicolon ";"
    .Equal "="
    .LParen "("
    .RParen ")"
    .Comma ","
    .Plus "+"
    .Minus "-"
    .LBrace "{"
    .RBrace "}"
    .Period "."
    .groupEnd()
  .in_group.keyword
    .If "if"
    .Then "then"
    .Elseif "elseif"
    .Else "else"
    .End "end"
    .For "for"
    .While "while"
    .Do "do"
    .Function "function"
    .Local "local"
    .Repeat "repeat"
    .Until "until"
    .Return "return"
    .with_tag.literal
      .Nil "nil"
      .True "true"
      .False "false"
      .tagEnd()
    .And "and"
    .Or "or"
    .Not "not"
    .groupEnd()

return Token
