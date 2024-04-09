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

/* ------------------------------------------------------------------------------------------------ Target::common_init
 */
void Target::common_init()
{
	prerequisites = TargetSet::create();
	dependents = TargetSet::create();
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::name
 */
str TargetSingle::name()
{
	return path;
}

/* ------------------------------------------------------------------------------------------------ TargetGroup::name
 */
str TargetGroup::name()
{
	return strl("group");
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::init
 */
void TargetSingle::init(str path_)
{
	common_init();
	path = path_;
	hash = path.hash();
}

/* ------------------------------------------------------------------------------------------------ TargetGroup::create
 */
void TargetGroup::init()
{
	common_init();
	hash = 0;
	targets = TargetSet::create();
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::exists
 */
b8 TargetSingle::exists()
{
	return ::path_exists(path);
}

/* ------------------------------------------------------------------------------------------------ TargetGroup::exists
 */
b8 TargetGroup::exists()
{
	for (auto& target : targets)
	{
		if (!target.exists())
			return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::modtime
 */
s64 TargetSingle::modtime()
{
	// TODO(sushi) what if our path is not null-terminated ?
	return ::modtime((char*)path.s);
}


/* ------------------------------------------------------------------------------------------------ TargetGroup::modtime
 *  Returns the oldest modtime. 
 *  TODO(sushi) I'm really not sure if this is correct yet, and I'm not sure if it will ever be 
 *  			used because i dont think anything will ever list the group as a prereq directly,
 *  			only targets that are in it.
 */
s64 TargetGroup::modtime()
{
	WARN("TargetGroup::modtime() was called but I am unsure if its behavior is correct so please check!!");
	
	s64 min = 9223372036854775807;
	for (auto& target : targets)
	{
		s64 t = target.modtime();
		if (t < min) 
			min = t;
	}

	return min;
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::is_newer_than
 */
b8 TargetSingle::is_newer_than(Target* t)
{
	return modtime() > t->modtime();
}

/* ------------------------------------------------------------------------------------------------ TargetGroup::is_newer_than
 */
b8 TargetGroup::is_newer_than(Target* t)
{
	return modtime() > t->modtime();
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::needs_built
 */
b8 TargetSingle::needs_built()
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
		if (prereq.is_newer_than(this))
		{
			TRACE("Target '", path, "' needs built because its prerequisite, '", prereq.name(), "', is newer.\n");
			return true;
		}
		TRACE("Prereq '", prereq.name(), "' is older than the target.\n");
	}

	return false;
}


/* ------------------------------------------------------------------------------------------------ TargetGroup::needs_built
 */
b8 TargetGroup::needs_built()
{
	for (auto& target : targets)
	{
		if (target.needs_built())
			return true;
	}
	return false;
}

/* ------------------------------------------------------------------------------------------------ Target::has_recipe
 */
b8 Target::has_recipe()
{
	return flags.test(Flags::HasRecipe);
}

/* ------------------------------------------------------------------------------------------------ Target::resume_recipe
 */
Target::RecipeResult Target::resume_recipe(lua_State* L)
{
	if (!lua_ref || !flags.test(Flags::HasRecipe))
	{
		error_nopath("Target '", name(), "' had resume_recipe() called on it, but either it doesn't reference a recipe to run!");
		return RecipeResult::Error;
	}

	lua_getglobal(L, lake_recipe_table);
	defer { lua_pop(L, 1); };

	// get the recipe function
	lua_rawgeti(L, -1, lua_ref);

	lua_getglobal(L, lake_coroutine_resume);

	// Resume the recipe by calling co.resume(f).
	// We only ever expect 2 returns:
	// the first being co.resume reporting if everything went okay,
	// the second either being nil, or not nil. 
	// A recipe, for now, is expected to return nil when it is finished. Internally
	// when we are asyncrocnously checking a process we return 'true' to indicate 
	// that the recipe is not yet finished, but we dont check for this explicitly.
	if (lua_pcall(L, 1, 2, 0))
	{
		print(lua_tostring(L, -1));
		return RecipeResult::Error;
	}

	b8 coroutine_success = lua_toboolean(L, -2);

	if (!coroutine_success)
	{
		// the second arg is the message given by whatever failure occured
		print(lua_tostring(L, -1));
		return RecipeResult::Error;
	}

	if (lua_isnil(L, -1))
		return RecipeResult::Finished;
	else
		return RecipeResult::InProgress;
}
