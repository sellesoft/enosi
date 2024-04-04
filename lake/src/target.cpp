#include "target.h"
#include "lake.h"
#include "platform.h"

#include "stdlib.h"
#include "assert.h"

#include "luahelpers.h"

#include "logger.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

/* ------------------------------------------------------------------------------------------------ target_hash
 */
u64 target_hash(const Target* t)
{
	return t->hash;
}

/* ------------------------------------------------------------------------------------------------ create
 */
Target Target::create(str path)
{
	Target out = {};
	out.path = path;
	out.hash = path.hash();
	out.prerequisites = TargetSet::create();
	out.dependents = TargetSet::create();
	return out;
}

/* ------------------------------------------------------------------------------------------------ exists
 */
b8 Target::exists()
{
	return ::path_exists(path);
}

/* ------------------------------------------------------------------------------------------------ modtime
 */
s64 Target::modtime()
{
	// TODO(sushi) what if our path is not null-terminated ?
	return ::modtime((char*)path.s);
}

/* ------------------------------------------------------------------------------------------------ is_newer_than
 */
b8 Target::is_newer_than(Target t)
{
	return modtime() > t.modtime();
}

/* ------------------------------------------------------------------------------------------------ needs_recipe
 */
b8 Target::needs_built()
{
	if (flags.test(Flags::PrerequisiteJustBuilt))
	{
		TRACE("Target '", path, "' needs built because the 'PrerequisiteJustBuilt' flag was set.\n");
		return true;
	}

	if (!exists())
	{
		TRACE("Target '", path, "' needs built because it does not exist on disk.\n");
		return true;
	}

	SCOPED_INDENT;
	for (Target& prereq : prerequisites)
	{
		if (prereq.is_newer_than(*this))
		{
			TRACE("Target '", path, "' needs built because its prerequisite, '", prereq.path, "', is newer.\n");
			return true;
		}
		TRACE("Prereq '", prereq.path, "' is older than the target.\n");
	}

	return false;
}

/* ------------------------------------------------------------------------------------------------ has_recipe
 */
b8 Target::has_recipe()
{
	return flags.test(Flags::HasRecipe);
}

/* ------------------------------------------------------------------------------------------------ resume_recipe
 */
Target::RecipeResult Target::resume_recipe(lua_State* L)
{
	lua_getglobal(L, lake_internal_table);
	lua_pushstring(L, "resume_recipe");
	lua_gettable(L, -2);

	defer { lua_pop(L, 2); };

	lua_pushlstring(L, (char*)path.s, path.len);
	if (lua_pcall(L, 1, 1, 0))
	{
		printv(lua_tostring(L, -1), "\n");
		lua_pop(L, 1);
		return RecipeResult::Error;
	}

	if (lua_toboolean(L, -1))
	{
		return RecipeResult::Finished; 
	}

	return RecipeResult::InProgress;
}
