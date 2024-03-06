local enum_output  = "src/generated/token.enum.h"
local str_output   = "src/generated/token.strings.h"
local kwmap_output = "src/generated/token.kwmap.h"

local write_out = function(path, out)
	local f = io.open(path, "w")

	if not f then
		print("failed to open file '"..path.."' for writing")
		return
	end

	f:write(out)
	f:close()
end


local tokens = 
{
	"Eof",

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
	"Caret",
	"Percent",
	"TildeEqual",
	"Plus",
	"Minus",
	"Solidus",
	"Asterisk",
	"LessThan",
	"LessThanOrEqual",
	"GreaterThan",
	"GreaterThanOrEqual",

	"Identifier",
	"Number",
	"String",
	"Whitespace",

	-- lake specific

	"Backtick",
	"Dollar",
	"ColonEqual",
	"QuestionMarkEqual",
}

local keyword_tokens = {
	{"Not",    "not"    },
	{"For",    "for"    },
	{"Do",     "do"     },
	{"If",     "if"     },
	{"Else",   "else"   },
	{"ElseIf", "elseif" },
	{"Then",   "then"   },  
	{"True",   "true"   },
	{"False",  "false"  },
	{"Nil",    "nil"    },
	{"And",    "and"    },
	{"Or",     "or"     },
	{"Local",  "local"  },
}

local out = [[
enum class tok
{
]]

for _,v in ipairs(tokens) do
	out = out.."\t"..v..",\n"	
end

for _,v in ipairs(keyword_tokens) do
	out = out.."\t"..v[1]..",\n"
end

out = out..[[
};
]]

write_out(enum_output, out)

out = [[
static const str tok_strings[] = 
{
]]

for _,v in ipairs(tokens) do
	out = out.."\tstrl(\""..v.."\"),\n"
end

for _,v in ipairs(keyword_tokens) do
	out = out.."\tstrl(\""..v[1].."\"),\n"
end

out = out..[[
};
]]

write_out(str_output, out)

out = [[
struct KWElem
{
	u64 hash;
	tok kind;
};

static tok is_keyword_or_identifier(str s)
{
	u64 hash = s.hash();
	switch (hash)
	{
]]

for _,v in ipairs(keyword_tokens) do
	out = out.."\t\t case static_string_hash(\""..v[1].."\"): return tok::"..v[1]..";\n"
end

out = out..[[
	}

	return tok::Identifier;
}
]]

write_out(kwmap_output, out)
