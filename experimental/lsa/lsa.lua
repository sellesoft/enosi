local Lexer = require "lexer"
local Token = require "token"


local lex = Lexer.new("test", 
[[
while true do 
  print("hello" + 1)
end
]])

while true do
  local tok = lex:nextToken()
  if tok.kind == Token.Kind.Eof then
    break
  end
  print(tok.kind)
end
