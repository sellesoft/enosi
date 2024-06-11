#include "luastate.h"

#include "logger.h"
#include "unicode.h"
#include "fs/file.h"
#include "containers/slice.h"

#undef stdout

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
};

namespace iro
{

Logger logger = Logger::create("luastate"_str, Logger::Verbosity::Info);

/* ------------------------------------------------------------------------------------------------ LuaState::init
 */
b8 LuaState::init()
{
	L = lua_open();
	luaL_openlibs(L);

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::deinit
 */
void LuaState::deinit()
{
	lua_close(L);
	L = nullptr;
}

/* ------------------------------------------------------------------------------------------------ LuaState::pop
 */
void LuaState::pop(s32 count)
{
	lua_pop(L, count);
}

/* ------------------------------------------------------------------------------------------------ LuaState::insert
 */
void LuaState::insert(s32 idx)
{
	lua_insert(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::gettop
 */
s32 LuaState::gettop()
{
	return lua_gettop(L);
}

/* ------------------------------------------------------------------------------------------------ LuaState::settop
 */
void LuaState::settop(s32 idx)
{
	return lua_settop(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::newtable
 */
void LuaState::newtable()
{
	lua_newtable(L);
}

/* ------------------------------------------------------------------------------------------------ LuaState::settable
 */
void LuaState::settable(s32 idx)
{
	lua_settable(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::gettable
 */
void LuaState::gettable(s32 idx)
{
	lua_gettable(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::setglobal
 */
void LuaState::setglobal(const char* name)
{
	lua_setglobal(L, name);
}

/* ------------------------------------------------------------------------------------------------ LuaState::getglobal
 */
void LuaState::getglobal(const char* name)
{
	lua_getglobal(L, name);
}

/* ------------------------------------------------------------------------------------------------ LuaState::objlen
 */
s32 LuaState::objlen(s32 idx)
{
	return lua_objlen(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::load
 */
b8 LuaState::load(io::IO* in, const char* name)
{
	static const s64 reader_buffer_size = 128;
	u8 reader_buffer[reader_buffer_size];
	struct D { u8* buffer; io::IO* io; } d {reader_buffer, in};

	auto reader = [](lua_State* L, void* data, size_t* size) -> const char*
	{
		D* d = (D*)data;
		*size = d->io->read({d->buffer, reader_buffer_size});
		return (const char*)d->buffer;
	};

	if (lua_load(L, reader, &d, name))
	{
		ERROR("load '", name, "' failed:\n", tostring(), "\n");
		pop();
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::loadbuffer
 */
b8 LuaState::loadbuffer(Bytes buffer, const char* name)
{
	auto loader = [](lua_State* L, void* data, size_t* size) -> const char* 
	{
		str* buffer = (str*)data;
		*size = buffer->len;
		return (char*)buffer->bytes;
	};

	if (lua_load(L, loader, &buffer, name))
	{
		ERROR("loadbuffer '", name, "' failed:\n", tostring(), "\n"); 
		pop();
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::loadfile
 */
b8 LuaState::loadfile(const char* path)
{
	if (luaL_loadfile(L, path))
	{
		ERROR("loadfile '", path, "' failed: \n", tostring(), "\n");
		pop();
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::loadstring
 */
b8 LuaState::loadstring(const char* s)
{
	if (luaL_loadstring(L, s))
	{
		ERROR("failed to load string:\n", tostring(), "\n");
		pop();
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::dofile
 */
b8 LuaState::dofile(const char* s)
{
	if(luaL_dofile(L, s))
	{
		ERROR("dofile(", s, ") failed:\n", tostring(), "\n");
		pop();
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::pcall
 */
b8 LuaState::pcall(s32 nargs, s32 nresults, s32 errfunc)
{
	if (lua_pcall(L, nargs, nresults, errfunc))
	{
		stackDump();
		ERROR(tostring(), "\n");
		pop();
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::callmeta
 */
b8 LuaState::callmeta(const char* name, s32 idx)
{
	if (!luaL_callmeta(L, idx, name))
		return false;
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::getfenv
 */
void LuaState::getfenv(s32 idx)
{
	lua_getfenv(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::setfenv
 */
b8 LuaState::setfenv(s32 idx)
{
	if (!lua_setfenv(L, idx))
	{
		ERROR("setfenv failed:\n", tostring(), "\n");
		pop();
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::tostring
 */
str LuaState::tostring(s32 idx)
{
	size_t len;
	const char* s = lua_tolstring(L, idx, &len);
	return {(u8*)s, len};
}

/* ------------------------------------------------------------------------------------------------ LuaState::tonumber
 */
f32 LuaState::tonumber(s32 idx)
{
	return lua_tonumber(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::toboolean
 */
b8 LuaState::toboolean(s32 idx)
{
	return lua_toboolean(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushstring
 */
void LuaState::pushstring(str s)
{
	// TODO(sushi) handle non-temrinated input
	lua_pushlstring(L, (char*)s.bytes, s.len);
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushlightuserdata
 */
void LuaState::pushlightuserdata(void* data)
{
	lua_pushlightuserdata(L, data);
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushinteger
 */
void LuaState::pushinteger(s32 i)
{
	lua_pushinteger(L, i);
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushvalue
 */
void LuaState::pushvalue(s32 idx)
{
	lua_pushvalue(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushnil
 */
void LuaState::pushnil()
{
	lua_pushnil(L);
}

/* ------------------------------------------------------------------------------------------------ LuaState::type
 */
int LuaState::type(s32 idx)
{
	return lua_type(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::typeName
 */
const char* LuaState::typeName(s32 idx)
{
	return lua_typename(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::next
 */
b8 LuaState::next(s32 idx)
{
	return lua_next(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::isnil
 */
b8 LuaState::isnil(s32 idx)
{
	return lua_isnil(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::isstring
 */
b8 LuaState::isstring(s32 idx)
{
	return lua_isstring(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::dump
 */
b8 LuaState::dump(io::IO* dest)
{
	auto writer = [](lua_State* L, const void* p, size_t sz, void* ud) ->int
	{
		io::IO* dest = (io::IO*)ud;

		if (!dest->write({(u8*)p, sz}))
			return 1;
		return 0;
	};

	if (lua_dump(L, writer, dest))
	{
		ERROR("dump failed\n");
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ writeLuaValue
 */
static void writeLuaValue(LuaState* L, io::IO* dest, int idx)
{
	int t = L->type(idx);

	switch (t)
	{
	case LUA_TTABLE:
		ERROR("writeLuaValue encountered a table, but this should have been passed to 'writeLuaTable'!\n");
		break;
	case LUA_TNIL:
		io::format(dest, "nil");
		break;
	case LUA_TNUMBER:
		io::format(dest, L->tonumber(idx));
		break;
	case LUA_TSTRING:
		io::formatv(dest, '"', io::SanitizeControlCharacters(L->tostring(idx)), '"');
		break;
	case LUA_TBOOLEAN:
		io::format(dest, (L->toboolean(idx)? "true" : "false"));
		break;
	case LUA_TTHREAD:
		io::format(dest, "<lua thread>");
		break;
	case LUA_TFUNCTION:
		io::format(dest, "<lua function>");
		break;
	case LUA_TUSERDATA:
		io::format(dest, "<lua userdata>");
		break;
	case LUA_TLIGHTUSERDATA:
		io::format(dest, "<lua lightuserdata>");
		break;
	default:
		io::formatv(dest, "<unhandle type in writeLuaValue: ", L->typeName(t), ">");
		break;
	}
}

/* ------------------------------------------------------------------------------------------------ writeLuaTable
 */
static void writeLuaTable(LuaState* L, io::IO* dest, int idx, u32 layers, u32 max_depth)
{
	auto indent = [layers, dest](s32 offset)
	{
		for (s32 i = 0; i < layers+offset; i++) 
			io::format(dest, "   ");
	};

	io::format(dest, '\n');
	indent(-1);
	io::format(dest, "{\n");
	defer { indent(-1); io::format(dest, '}'); };


	L->pushvalue(idx);
	L->pushnil();

	while (L->next(-2))
	{
		int kt = L->type(-2);
		int vt = L->type(-1);

		indent(0);

		writeLuaValue(L, dest, -2);

		io::format(dest, " = ");

		if (vt == LUA_TTABLE)
		{
			if (layers >= max_depth)
				io::format(dest, "...\n");
			else
			{
				writeLuaTable(L, dest, -1, layers + 1, max_depth);
				io::format(dest, '\n');
			}
		}
		else
		{
			writeLuaValue(L, dest, -1);
			io::format(dest, '\n');
		}
		L->pop();
	}
	L->pop();
}

/* ------------------------------------------------------------------------------------------------ LuaState::require
 */
b8 LuaState::require(str modname, u32* out_ret)
{
	u32 top = gettop();

	getglobal("require");
	pushstring(modname);
	if (!pcall(1, LUA_MULTRET))
	{
		ERROR("failed to require module '", modname, "':\n", tostring());
		return false;
	}

	if (out_ret)
		*out_ret = gettop() - top;

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::stackDump
 */
void LuaState::stackDump(io::IO* dest, u32 max_depth)
{
	s32 top = gettop();

	for (s32 i = 1; i <= top; i++)
	{
		int t = type(i);
		io::formatv(dest, "i ", i, ": ");
		if (t == LUA_TTABLE)
			writeLuaTable(this, dest, i, 1, max_depth);
		else
			writeLuaValue(this, dest, i);
		io::format(dest, '\n');
	}
}

void LuaState::stackDump(u32 max_depth)
{
	stackDump(&fs::stdout, max_depth);
}

/* ------------------------------------------------------------------------------------------------ LuaState::installDebugHook
 */
void LuaState::installDebugHook()
{
	auto hook = [](lua_State* L, lua_Debug* ar) -> void
	{
		lua_getinfo(L, "nSlu", ar);

		b8 leaving_func = ar->event == LUA_HOOKRET;

		if (ar->event == LUA_HOOKCALL)
			INFO("calling ");
		else
			INFO("leaving ");

		switch (cStrHash(ar->what))
		{
		case "main"_hashed:
			INFO("main chunk\n");
			break;

		case "Lua"_hashed:
			INFO("lua function\n");
			break;

		case "C"_hashed:
			INFO("C function\n");
			break;

		case "tail"_hashed:
			INFO("tail call\n");
			break;
		}

		if (ar->name)
		{
			INFO("function name: ", ar->name, "\n");
		}
	};

	if (lua_sethook(L, hook, LUA_MASKCALL | LUA_MASKRET, 0))
	{
		ERROR("failed to install debug hook!\n");
	}
}

}
