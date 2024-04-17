--
--
-- Command line arguments declarations and code generation.
--
--

local buffer = require "string.buffer"

local outpath = "src/generated/cliargparser.h"

local args =
{
	{
		switch = "--verbosity",
		enum = "VerboseBig",
		description = "Changes the verbosity of the internal logger.",
		options =
		{
			-- pair of option and C function that will handle that option being chosen
			{ "trace",  "Trace"  },
			{ "debug",  "Debug"  },
			{ "info",   "Info"   },
			{ "notice", "Notice" },
			{ "warn",   "Warn"   },
			{ "error",  "Error"  },
			{ "fatal",  "Fatal"  }
		},
	},
	{
		switch = "-v",
		enum = "VerboseSmall",
		description = "Sets the internal logger's verbosity to 'debug'.",
	},
	{
		switch = "-j",
		enum = "JobsSmall",
	},
	{
		switch = "--max-jobs",
		enum = "JobsBig",
	},
	{
		switch = "--print-transformed",
		enum = "PrintTransformed",
		description = "Logs the lakefile after syntax sugar has been removed."
	}
}

local trie = {child_count = 0}

-- returns the last node in the trie (the last letter)
local add_to_trie = function(trie, s)
	local node = trie
	for i=1,#s do
		local c = s:sub(i,i)
		if not node[c] then 
			node[c] = {child_count = 0}
		end
		node.child_count = node.child_count + 1
		node = node[c]
	end
	return node
end

local CGen =
{
	buffer = buffer.new(),

	depth = 0,

	append_indentation = function(self)
		for _=1,self.depth do
			self.buffer:put(" ")
		end
	end,

	begin_enum = function(self, name)
		self:append_indentation()
		self.buffer:put("enum ", name, "\n")
		self:begin_scope()
	end,

	append_enum_element = function(self, name)
		self:append_indentation()
		self.buffer:put(name, ",\n")
	end,

	end_enum = function(self)
		self.depth = self.depth - 1
		self:append_indentation()
		self.buffer:put("};\n")
	end,

	begin_switch = function(self, c)
		self:append_indentation()
		self.buffer:put("switch (", c, ")\n")
		self:begin_scope()
	end,

	end_switch = function(self)
		self:end_scope()
	end,

	begin_case = function(self, c)
		self:append_indentation()
		self.buffer:put("case '", c, "':\n")
		self.depth = self.depth + 1
	end,

	end_case = function(self)
		self:append_indentation()
		self.buffer:put("break;\n")
		self.depth = self.depth - 1
	end,

	begin_default_case = function(self)
		self:append_indentation()
		self.buffer:put("default:\n")
		self.depth = self.depth + 1
	end,

	end_default_case = function(self)
		self:append_indentation()
		self.buffer:put("break;\n")
		self.depth = self.depth - 1
	end,

	begin_if = function(self, cond)
		self:append_indentation()
		self.buffer:put("if (", cond, ")\n")
		self:begin_scope()
	end,

	-- expected to match an 'if'
	-- also ended with 'end_if'
	begin_else = function(self)
		self:end_if()
		self:append_indentation()
		self.buffer:put("else\n")
		self:begin_scope()
	end,

	begin_else_if = function(self, cond)
		self:end_if()
		self:append_indentation()
		self.buffer:put("else if (", cond, ")\n")
		self:begin_scope()
	end,

	end_if = function(self)
		self:end_scope()
	end,

	begin_function = function(self, return_type, name, ...)
		self:append_indentation()
		self.buffer:put(return_type, " ", name, "(")
		local args = {...}
		if #args ~= 0 then
			for _,arg in ipairs(args) do
				self.buffer:put(arg)
			end
		end
		self.buffer:put(")\n")
		self:begin_scope()
	end,

	end_function = function(self)
		self:end_scope()
	end,

	begin_infinite_loop = function(self)
		self:append_indentation()
		self.buffer:put("for (;;)\n")
		self:begin_scope()
	end,

	end_infinite_loop = function(self)
		self:end_scope()
	end,

	begin_scope = function(self)
		self:append_indentation()
		self.buffer:put("{\n")
		self.depth = self.depth + 1
	end,

	end_scope = function(self)
		self.depth = self.depth - 1
		self:append_indentation()
		self.buffer:put("}\n")
	end,

	-- note that this appends by line 
	append = function(self, ...)
		self:append_indentation()
		self.buffer:put(...)
		self.buffer:put("\n")
	end,
}
CGen.__index = CGen

CGen.new = function()
	local o = {}
	setmetatable(o, CGen)
	o.buffer = buffer.new()
	o.depth = 0
	return o
end

local arg_type_enum = CGen.new()
local parse_function = CGen.new()

arg_type_enum:begin_enum("CLIArgType")
arg_type_enum:append_enum_element("Unknown")

local walk

for _,arg in ipairs(args) do

	local node = add_to_trie(trie, arg.switch)

	arg_type_enum:append_enum_element(arg.enum)

	node.fin = function()
		parse_function:append("return CLIArgType::", arg.enum, ";")
	end

	if arg.options then
		for _,option in ipairs(arg.options) do
			arg_type_enum:append_enum_element("Option"..option[2])
			local node = add_to_trie(trie, option[1])
			node.fin = function()
				parse_function:append("return CLIArgType::Option", option[2], ";")
			end
		end
	end
end

-- walk the trie we built and construct the code
walk = function(node)
	parse_function:begin_switch("*arg")
	for k,v in pairs(node) do
		if k == "fin" then
			if node.child_count == 0 then
				-- if this is a terminal node then there's no more known arguments starting 
				-- with these characters, so we can early out knowing that this arg is unknown
				parse_function:begin_default_case()
					parse_function:append("return CLIArgType::Unknown;")
				parse_function:end_default_case()

				parse_function:begin_case("\\0")
					v()
				parse_function:end_case()
			else
				parse_function:begin_default_case()
					v()
				parse_function:end_default_case()
			end
		elseif k ~= "child_count" then
			parse_function:begin_case(k)
			parse_function:append("arg += 1;")
			walk(v)
			parse_function:end_case()
		end
	end
	parse_function:end_switch()
end

parse_function:begin_function("static CLIArgType", "parse_cli_arg", "const char* arg")

walk(trie)

parse_function:append("return CLIArgType::Unknown;")

parse_function:end_function()
arg_type_enum:end_enum()

local file = io.open(outpath, "w")

if not file then
	io.write("failed to open path '", outpath, "' for writing!\n")
	os.exit(1)
end

file:write(arg_type_enum.buffer:get())
file:write(parse_function.buffer:get())
file:close()

