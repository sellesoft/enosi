#include "target.h"
#include "lake.h"
#include "platform.h"

#include "stdlib.h"
#include "assert.h"

#include "luahelpers.h"

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
		return true;

	if (!exists())
		return true;

	for (Target& prereq : prerequisites)
		if (prereq.is_newer_than(*this))
			return true;

	return false;
}

/* ------------------------------------------------------------------------------------------------ has_recipe
 */
b8 Target::has_recipe(lua_State* L)
{
	lua_getglobal(L, lake_targets_table);
	defer { lua_pop(L, 1); };

	lua_pushlstring(L, (char*)path.s, path.len);
	lua_gettable(L, -2);

	if (lua_type(L, -1) == LUA_TNIL)
	{
		error_nopath("target_get_recipe(): attempted to get target at path '", path, "' but it somehow doesn't exist in the target table\n");
		exit(1);
	}

	lua_pushstring(L, "recipe");
	lua_gettable(L, -2);
	
	if (lua_type(L, -1) == LUA_TNIL)
		return false;
	else
		return true;
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
		printv("lua error while running recipe for target '", path, "':\n", lua_tostring(L, -1), "\n");
		lua_pop(L, 1);
		return RecipeResult::Error;
	}

	if (lua_toboolean(L, -1))
	{
		return RecipeResult::Finished;
	}

	return RecipeResult::InProgress;
}

/* ------------------------------------------------------------------------------------------------ remove
 */
void Target::remove()
{
	for (Target& t : prerequisites)
		t.dependents.remove(this);

	for (Target& t : dependents)
		t.prerequisites.remove(this);
}

