local output = "src/generated/tokens.h"

local tokens = 
{
	"NULL",

	"Colon",
	"Semicolon", 
	"Equal",
	"Comma",
	"ParenLeft",
	"ParenRight",
	"BraceLeft",
	"BraceRight",
	"SquareLeft",
	"SquareRight",
	"Ellises",
	"DotDouble",
	"Dot",
	"Asterisk",
	"Caret",
	"Percent",
	"TildeEqual",
	"Plus",
	"Minus",
	"LessThan",
	"LessThanOrEqual",
	"GreaterThan",
	"GreaterThanOrEqual",

	"Not",
	"For",
	"Do",
	"If",
	"Else",
	"ElseIf",
	"Then",  
	"True",
	"False",
	"Nil",
	"And",
	"Or",

	"Number",
	"String",

	-- lake specific

	"Backtick",
	"Dollar",
	"ColonEqual",
	"QuestionMarkEqual",
}

local out = [[
enum class tok
{
]]

for _,v in ipairs(tokens) do
	out = out.."\t"..v..",\n"	
end

out = out..[[
};

static const str tok_strings[] = 
{
]]

for _,v in ipairs(tokens) do
	out = out.."\tstrl(\""..v.."\"),\n"
end

out = out..[[
};
]]

local f = io.open(output, "w")

if not f then
	print("failed to open file '"..output.."' for writing")
	return
end

f:write(out)
f:close()
