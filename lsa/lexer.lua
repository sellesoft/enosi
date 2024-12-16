--- 
--- Turns some input text in Tokens.
---

local cmn = require "common"
local Scanner = require "Scanner"
local Token = require "token"

--- Generates tokens from some input text.
---@class lsa.Lexer
--- The underlying scanner.
---@field scanner iro.Scanner
--- List of tokens we've found.
---@field tokens List
local Lexer = cmn.Type.make()

Lexer.Done = {}

---@return lsa.Lexer
Lexer.new = function(textname, text)
  local o = {}
  o.textname = textname
  o.scanner = Scanner.new(text)
  o.tokens = cmn.List{}

  local c = cmn.pco(Lexer.nextToken)
  local co = require "coroutine"

  o.nextToken = function(self, ...)
    if co.status(c) == "dead" then
      return Lexer.Done
    end
    local result = { co.resume(c, self, ...) }
    if not result[1] then
      error(result[2])
    end
    return unpack(result, 2)
  end

  return setmetatable(o, Lexer)
end

---@return lsa.Token
Lexer.pushToken = function(self, kind, start, stop)
  local t = Token.new(kind, start, stop)
  self.tokens:push(t)
  return t
end

Lexer.skipWhitespaceAndComments = function(self)
  local scanner = self.scanner
  while true do
    local start, stop = scanner:pattern "^%s*"
    if start then
      if stop-start+1 > 0 then
        self:pushToken(Token.Kind.Whitespace, start, stop)
        scanner.offset = stop + 1
      end
    end

    print(scanner.text:sub(scanner.offset))

    local start, stop = scanner:pattern "^--[^\n]*"
    if start then
      scanner.offset = stop + 1
    else
      break
    end
  end
end

---@return lsa.Token
Lexer.nextToken = function(self)
  local scanner = self.scanner
  local checkToken = function()
    if scanner.offset > #scanner.text then
      return self:pushToken(Token.Kind.Eof, #scanner.text, #scanner.text)
    end

    -- lol
    for i=1,Token.max_str_len do
      local kind = Token.str_to_kind[scanner.text:sub(1,i)]
      if kind then
        local t = self:pushToken(
          kind, 
          scanner.offset, 
          scanner.offset + i)
        scanner.offset = scanner.offset + i
        return t
      end
    end
    
    local start = scanner.offset

    if scanner:checkNumber() then
      return self:pushToken(Token.Kind.Number, start, scanner.offset)
    end

    if scanner:checkString() then
      return self:pushToken(Token.Kind.String, start+1, scanner.offset-1)
    end

    if scanner:checkIdentifier() then
      return self:pushToken(Token.Kind.Identifier, start, scanner.offset)
    end

    scanner:errorHere("unhandled lex")
  end

  while true do
    self:skipWhitespaceAndComments()

    local tok = checkToken()
    if tok.kind == Token.Kind.Eof then
      return tok
    else
      cmn.co.yield(tok)
    end
  end
end

Lexer.getRaw = function(self, tok)
  return self.scanner.text:sub(tok.loc, tok.loc+tok.len-1)
end

return Lexer
