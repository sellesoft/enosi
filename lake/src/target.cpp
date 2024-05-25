#include "target.h"
#include "lake.h"

#include "stdlib.h"
#include "assert.h"

#include "luahelpers.h"

#include "iro/logger.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

static Logger logger = Logger::create("lake.target"_str, Logger::Verbosity::Warn);

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
	build_node = nullptr;
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::name
 */
str Target::name()
{
	assert(kind != Kind::Unknown);
	switch (kind)
	{
		case Kind::Single: return single.path.buffer.asStr();
		case Kind::Group: return "group"_str;
	}
	return {};
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::init
 */
void Target::init_single(str path)
{
	common_init();
	kind = Kind::Single;
	auto temp = fs::Path::from(path);
	mem::copy(&single.path, &temp, sizeof(fs::Path)); 
	// TODO(sushi) make a lexical absolute thing so that this can still work in that case.
	//             this needs to be done because i use cpp to get the headers a cpp 
	//             file depends on, which gives the paths relative to where they're 
	//             being included from, so if i dont sanitize them multiple targets
	//             will be created for the same file since targets are based on the path.
	if (single.path.exists())
		single.path.makeAbsolute();
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

/* ------------------------------------------------------------------------------------------------ Target::deinit
 */
void Target::deinit()
{
	prerequisites.destroy();
	dependents.destroy();
	if (kind == Kind::Group)
		group.targets.destroy();
	else
		single.path.destroy();
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::exists
 */
b8 Target::exists()
{
	assert(kind != Kind::Unknown);
	switch (kind)
	{
		case Kind::Single: return single.path.exists();
		
		case Kind::Group:
			for (auto& target : group.targets)
			{
				if (!target.exists())
					return false;
			}
			return true;
	}
	return {};
}

/* ------------------------------------------------------------------------------------------------ TargetSingle::modtime
 */
s64 Target::modtime()
{
	assert(kind != Kind::Unknown);
	switch (kind)
	{
		// TODO(sushi) what if our path is not null-terminated ?
		case Kind::Single: return fs::FileInfo::of(single.path.buffer.asStr()).last_modified_time.s;

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
	return {};
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
		ERROR("Target '", name(), "' had resume_recipe() called on it, but it has no recipe!");
		return RecipeResult::Error;
	}

	auto cwd = fs::Path::cwd(); // ugh man
	defer { cwd.chdir(); cwd.destroy(); };

	recipe_working_directory.chdir();

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
	// when we are asyncronously checking a process we return 'true' to indicate 
	// that the recipe is not yet finished, but we dont check for this explicitly.
	if (lua_pcall(L, 1, 2, 0))
	{
		ERROR(lua_tostring(L, -1), "\n");
		return RecipeResult::Error;
	}

	b8 coroutine_success = lua_toboolean(L, -2);

	if (!coroutine_success)
	{
		// the second arg is the message given by whatever failure occured
		ERROR(lua_tostring(L, -1), "\n");
		return RecipeResult::Error;
	}

	defer { lua_pop(L, 2); };

	if (lua_isnil(L, -1))
		return RecipeResult::Finished;
	else
		return RecipeResult::InProgress;
}

/* ------------------------------------------------------------------------------------------------ Target::update_dependents
 */
void Target::update_dependents(TargetList& build_queue, b8 mark_just_built)
{
	switch (kind)
	{
		case Kind::Single: {
			for (auto& dependent : dependents)
			{
				if (mark_just_built)
					dependent.flags.set(Flags::PrerequisiteJustBuilt);
				if (dependent.unsatified_prereq_count == 1)
				{
					INFO("Dependent '", dependent.name(), "' has no more unsatisfied prerequisites, adding it to the build queue.\n");
					dependent.build_node = build_queue.pushTail(&dependent);
					dependent.unsatified_prereq_count = 0;
				}
				else if (dependent.unsatified_prereq_count != 0)
				{
					
					dependent.unsatified_prereq_count -= 1;
					TRACE("Target '", name(), "' has ", dependent.unsatified_prereq_count, " unsatisfied prereqs left.\n");
				}
			}
		} break;

		case Kind::Group: {
			for (auto& target : group.targets)
			{
				target.update_dependents(build_queue, mark_just_built);
			}
		} break;
	}
}


s64 iro::io::format(io::IO* io, Target& t)
{
	switch (t.kind)
	{
	case Target::Kind::Unknown:
		io->write("Unknown target"_str);
		break;

	default:
		io::formatv(io, 
				"Target '", t.name(), "':",
				"\n     hash: ", t.hash,
				"\n  lua_ref: ", t.lua_ref,
				"\n unsatisf: ", t.unsatified_prereq_count,
				"\n      cwd: ", t.recipe_working_directory,
				"\n* prereqs:");

		for (auto& p : t.prerequisites)
			io::formatv(io, "\n    ", p.name());

		io::formatv(io, "\n*   deps:");

		for (auto& d : t.dependents)
			io::formatv(io, "\n    ", d.name());
		break;
	}

	return 0;
}
