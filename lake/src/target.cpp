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
str Target::name()
{
	switch (kind)
	{
		case Kind::Single: return single.path;
		case Kind::Group: return strl("group");
	}
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::init
 */
void Target::init_single(str path)
{
	common_init();
	kind = Kind::Single;
	single.path = path;
	hash = path.hash();
}

/* ------------------------------------------------------------------------------------------------ TargetGroup::create
 */
void Target::init_group()
{
	common_init();
	kind = Kind::Group;
	hash = 0;
	group.targets = TargetSet::create();
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::exists
 */
b8 Target::exists()
{
	switch (kind)
	{
		case Kind::Single: return ::path_exists(single.path);
		
		case Kind::Group:
			for (auto& target : group.targets)
			{
				if (!target.exists())
					return false;
			}
			return true;
	}
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::modtime
 */
s64 Target::modtime()
{
	switch (kind)
	{
		// TODO(sushi) what if our path is not null-terminated ?
		case Kind::Single: return ::modtime((char*)single.path.s);

		case Kind::Group: {
			s64 min = 9223372036854775807;
			for (auto& target : group.targets)
			{
				s64 t = target.modtime();
				if (t < min) 
					min = t;
			}
			return min;
		}break;
	}
}


/* ------------------------------------------------------------------------------------------------ TargetSingle::is_newer_than
 */
b8 Target::is_newer_than(Target* t)
{
	return modtime() > t->modtime();
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::needs_built
 */
b8 Target::needs_built()
{
	switch (kind)
	{
		case Kind::Group:
		case Kind::Single: {
			if (flags.test(Flags::PrerequisiteJustBuilt))
			{
				TRACE("Target '", single.path, "' needs built because the 'PrerequisiteJustBuilt' flag was set.\n");
				return true;
			}

			if (!exists())
			{
				TRACE("Target '", single.path, "' needs built because it does not exist on disk.\n");
				return true;
			}

		
			SCOPED_INDENT;
			for (Target& prereq : prerequisites)
			{
				if (prereq.is_newer_than(this))
				{
					TRACE("Target '", single.path, "' needs built because its prerequisite, '", prereq.name(), "', is newer.\n");
					return true;
				}
				TRACE("Prereq '", prereq.name(), "' is older than the target.\n");
			}

		} break;

	//	case Kind::Group: 
	//		for (auto& target : group.targets)
	//		{
	//			if (target.needs_built())
	//				return true;
	//		}
	//		break;
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

	lua_getglobal(L, lake_coroutine_resume);

	// get the recipe function
	lua_rawgeti(L, -2, lua_ref);

	// Resume the recipe by calling co.resume(f).
	// We only ever expect 2 returns:
	// the first being co.resume reporting if everything went okay,
	// the second either being nil, or not nil. 
	// A recipe, for now, is expected to return nil when it is finished. Internally
	// when we are asyncrocnously checking a process we return 'true' to indicate 
	// that the recipe is not yet finished, but we dont check for this explicitly.
	if (lua_pcall(L, 1, 2, 0))
	{
		printv(lua_tostring(L, -1), "\n");
		return RecipeResult::Error;
	}

	b8 coroutine_success = lua_toboolean(L, -2);

	if (!coroutine_success)
	{
		// the second arg is the message given by whatever failure occured
		printv(lua_tostring(L, -1), "\n");
		return RecipeResult::Error;
	}

	defer { lua_pop(L, 2); };

	if (lua_isnil(L, -1))
		return RecipeResult::Finished;
	else
		return RecipeResult::InProgress;
}
