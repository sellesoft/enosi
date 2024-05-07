--
--
-- Script for autogenerating the cdefs needed to use the lppclang api from lua.
--
--

local header_path = "src/lppclang.h"

local header = io.open(header_path, "r")

if not header then
	error("failed to open '"..header_path.."' for reading")
end

local api = ""
local take = false

while true do
	local line = header:read("L")

	if not line then
		break
	end

	if line:match("//!ffiapi end") then
		take = false
	end

	if take then
		api = api..line
	end

	if line:match("//!ffiapi start") then
		take = true
	end
end

-- preprocess with cpp
-- TODO(sushi) this isnt portable
--             maybe make a c preprocessor module for iro later

local tmpname = os.tmpname()
local tmpfile = io.open(tmpname, "w")
if not tmpfile then
	error("failed to open tempfile '"..tmpname.."' for writing")
end
tmpfile:write(api)
tmpfile:close()

local result = io.popen("cpp -P "..tmpname, "r")
if not result then
	error("failed to open cpp process")
end

local cdef = result:read("*a")

print(cdef)
