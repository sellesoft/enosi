#include "lake.h"

#include "stdlib.h"
#include "assert.h"
#include "ctype.h"

#include "lexer.h"
#include "parser.h"

#include "target.h"

#include "iro/logger.h"
#include "luahelpers.h"

#include "generated/cliargparser.h"
#include "generated/lakeluacompiled.h"

#include "assert.h"

#include "string.h"

#include "iro/time/time.h"

#include "iro/process.h"
#include "iro/fs/glob.h"

#undef stdout
#undef stderr

using namespace iro;

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

int lua__importFile(lua_State* L);
int lua__cwd(lua_State* L);
}

static Logger logger = Logger::create("lake"_str, 

#if LAKE_DEBUG
		Logger::Verbosity::Debug);
#else
		Logger::Verbosity::Warn);
#endif

Lake lake; // global for now, maybe not later 

struct ActiveProcess
{
	Process process;
	fs::File stdout;
	fs::File stderr;
};

Pool<ActiveProcess> active_process_pool = {};

/* ------------------------------------------------------------------------------------------------ Lake::init
 */
b8 Lake::init(const char** argv_, int argc_, mem::Allocator* allocator)
{
	assert(argv_ && allocator);

	INFO("Initializing Lake.\n");
	SCOPED_INDENT;

	max_jobs = 1;

	argc = argc_;
	argv = argv_;

	lua_cli_args = LuaCLIArgList::create(allocator);
	active_process_pool = Pool<ActiveProcess>::create(allocator);
	cwd_stack = Array<fs::Path>::create(allocator);

	initpath = nil;
	if (!process_argv(&initpath))
		return false;
	b8 resolved = resolve(initpath, "lakefile"_str);

	if (resolved)
		DEBUG("no file specified on cli, searching for 'lakefile'\n");
	else
		DEBUG("file '", initpath, "' was specified as file on cli\n");

	using namespace fs;

	if (!fs::Path::exists(initpath))
	{
		if (resolved)
			FATAL("no file was specified (-f) and no file with the default name 'lakefile' could be found\n");
		else
			FATAL("failed to find specified file (-f) '", initpath, "'\n");
		return false;
	}

	lua_cli_arg_iterator = lua_cli_args.head;

	DEBUG("Creating target pool and target lists.\n");

	target_pool    = TargetPool::create(allocator);
	build_queue    = TargetList::create(allocator);
	active_recipes = TargetList::create(allocator);

	DEBUG("Loading lua state.\n");

	L = lua_open();
	luaL_openlibs(L);

	// TODO(sushi) do this in a cleaner way later.
	//             this function has to leave the result of the file on the 
	//             stack and i dont think thats possible with a luajit ffi func.
	lua_pushcfunction(L, lua__importFile);
	lua_setglobal(L, "lua__importFile"); 
	lua_pushcfunction(L, lua__cwd);
	lua_setglobal(L, "lua__cwd"); 

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lake::deinit
 */
void Lake::deinit()
{
	lua_close(L);
	active_recipes.destroy();
	build_queue.destroy();

	for (auto& t : targets)
		t.deinit();
	targets.destroy();
	target_pool.destroy();

	cwd_stack.destroy();
	active_process_pool.destroy();
	lua_cli_args.destroy();
}

struct ArgIter
{
	const char** argv = nullptr;
	u64 argc = 0;
	u64 idx = 0;

	str current = nil;

	ArgIter(const char** argv, u32 argc) : argv(argv), argc(argc) 
	{  
		idx = 1;
		next();
	}

	void next()
	{
		if (idx == argc)
		{
			current = nil;
			return;
		}

		current = { (u8*)argv[idx++] };
		current.len = strlen((char*)current.bytes);
	}
};

/* ------------------------------------------------------------------------------------------------ process_max_jobs_arg
 */
b8 process_max_jobs_arg(Lake* lake, ArgIter* iter)
{
	str mjarg = iter->current;

	iter->next();
	if (isnil(iter->current))
	{
		FATAL("expected a number after '--max-jobs'\n");
		return false;
	}

	str arg = iter->current;

	u8* scan = arg.bytes;

	while (*scan)
	{
		if (!isdigit(*scan))
		{
			FATAL("given argument '", arg, "' after '", mjarg, "' must be a number\n");
			return false;
		}

		scan += 1;
	}

	lake->max_jobs = atoi((char*)arg.bytes);

	DEBUG("max_jobs set to ", lake->max_jobs, " via command line argument\n");

	lake->max_jobs_set_on_cli = true;

	return true;
}

/* ------------------------------------------------------------------------------------------------ process_arg_double_dash
 */
b8 process_arg_double_dash(Lake* lake, str arg, str* initfile, ArgIter* iter)
{
	switch (arg.hash())
	{
	case "verbosity"_hashed:
		{
			iter->next();
			if (isnil(iter->current))
			{
				FATAL("expected a verbosity level after '--verbosity'\n");
				return false;
			}

			switch (iter->current.hash())
			{

#define verbmap(s, level) \
			case GLUE(s, _hashed): \
				DEBUG("Logger verbosity level set to " STRINGIZE(level) " via command line argument.\n"); \
				logger.verbosity = Logger::Verbosity::level;\
				break;
			
			verbmap("trace", Trace);
			verbmap("debug", Debug);
			verbmap("info", Info);
			verbmap("notice", Notice);
			verbmap("warn", Warn);
			verbmap("error", Error);
			verbmap("fatal", Fatal);
			default:
				FATAL("expected a verbosity level after '--verbosity'\n");
				return false;
#undef verbmap

			}
		}
		break;

	case "print-transformed"_hashed:
		lake->print_transformed = true;
		break;

	case "max-jobs"_hashed:
		if (!process_max_jobs_arg(lake, iter))
			return false;
		break;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ process_arg_single_dash
 */
b8 process_arg_single_dash(Lake* lake, str arg, str* initfile, ArgIter* iter)
{
	switch (arg.hash())
	{
	case "v"_hashed:
		logger.verbosity = Logger::Verbosity::Debug;
		DEBUG("Logger verbosity set to Debug via command line argument.\n");
		break;

	case "j"_hashed:
		if (!process_max_jobs_arg(lake, iter))
			return false;
		break;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lake::process_argv
 */
b8 Lake::process_argv(str* initfile)
{
	INFO("Processing cli args.\n");
	SCOPED_INDENT;

	for (ArgIter iter(argv, argc); notnil(iter.current); iter.next())
	{
		str arg = iter.current;
		if (arg.len > 1 && arg.bytes[0] == '-')
		{
			if (arg.len > 2 && arg.bytes[1] == '-')
			{
				if (!process_arg_double_dash(this, arg.sub(2), initfile, &iter))
					return false;
			}
			else
			{
				if (!process_arg_single_dash(this, arg.sub(1), initfile, &iter))
					return false;
			}
		}
		else
		{
			lua_cli_args.push(argv + iter.idx);
			DEBUG("Encountered unknown cli arg '", arg, "' so it has been added to the lua cli args list.\n");
		}
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lake::run
 */
b8 Lake::run()
{

	INFO("Loading lake lua module.\n");

#if LAKE_DEBUG
	if (luaL_loadfile(L, "lake/src/lake.lua") || lua_pcall(L, 0, 5, 0))
	{
		printf("%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return false;
	}
#else
	if (luaL_loadbuffer(L, (char*)luaJIT_BC_lake, luaJIT_BC_lake_SIZE, "lake.lua") ||
		lua_pcall(L, 0, 5, 0))
	{
		ERROR(lua_tostring(L, -1));
		lua_pop(L, 1);
		return false;
	}
#endif

	INFO("Setting lua globals.\n");

	lua_setglobal(L, lake_coroutine_resume);
	lua_setglobal(L, lake_recipe_table);
	lua_setglobal(L, lake_targets_table);
	lua_setglobal(L, lake_internal_table); 
	lua_setglobal(L, "lake");

	INFO("Starting parser on lakefile.\n");

	// TODO(sushi) we can just load the program into lua by token 
	//             using the loader callback thing in lua_load
	io::Memory prog;
	prog.open();

	if (!parseFile(initpath, &prog))
		return false;

	if (print_transformed)
		printf("%.*s\n", prog.len, prog.buffer);

	INFO("Executing transformed lakefile.\n"); 
	if (luaL_loadbuffer(L, (char*)prog.buffer, prog.len, "lakefile") || lua_pcall(L, 0, 0, 0))
	{
		printf("%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return false;
	}

	prog.close();

	//for (auto& t : target_pool)
	//	io::formatv(&fs::stdout, t, "\n\n");

	INFO("Beginning build loop.\n");

	for (u64 build_pass = 0;;)
	{
		if (!build_queue.isEmpty() && max_jobs - active_recipe_count)
		{
			build_pass += 1;

			INFO("Entering build pass ", build_pass, "\n");
			SCOPED_INDENT;

			while (!build_queue.isEmpty() && max_jobs - active_recipe_count)
			{
				Target* target = build_queue.front();

				INFO("Checking target '", target->name(), "' from build queue.\n"); 
				SCOPED_INDENT;

				if (target->needs_built())
				{
					INFO("Target needs built, adding to active recipes list\n");
					target->active_recipe_node = active_recipes.pushHead(target);
					active_recipe_count += 1;
					build_queue.remove(target->build_node);
				}
				else
				{
					INFO("Target does not need built, removing from build queue and decrementing all dependents unsatified prereq counts.\n");
					SCOPED_INDENT;
					build_queue.remove(target->build_node);
					target->update_dependents(build_queue, false);
				}
			}
		}
		
		if (active_recipes.isEmpty() && build_queue.isEmpty())
		{
			INFO("Active recipe list and build queue are empty, we must be finished.\n");
			break;
		}

		for (auto& t : active_recipes)
		{
			switch (t.resume_recipe(L))
			{
				case Target::RecipeResult::Finished: {
					TRACE("Target '", t.name(), "'s recipe has finished.\n");
					SCOPED_INDENT;

					active_recipes.remove(t.active_recipe_node);
					t.update_dependents(build_queue, true);
					active_recipe_count -= 1;
				} break;

				case Target::RecipeResult::Error: {
					ERROR("recipe error\n");
					return false;
				} break;

				case Target::RecipeResult::InProgress: {

				} break;
			}
		}
	}

	lua_getglobal(L, lake_internal_table);
	lua_pop(L, 2);

	return true;
}

#if LAKE_DEBUG

void assert_group(Target* target)
{
	if (target->kind != Target::Kind::Group)
	{
		FATAL("Target '", (void*)target, "' failed group assertion\n");
		exit(1);
	}
}

void assert_single(Target* target)
{
	if (target->kind != Target::Kind::Single)
	{
		FATAL("Target '", (void*)target, "' failed single assertion\n");
		exit(1);
	}
}

#else
void assert_group(Target*){}
void assert_single(Target*){}
#endif

/* ================================================================================================ Lua API
 *  Implementation of the api used in the lua lake module.
 */
extern "C"
{

/* ------------------------------------------------------------------------------------------------ lua__createSingleTarget
 */
Target* lua__createSingleTarget(str path)
{
	Target* t = lake.target_pool.add();
	t->init_single(path);
	t->build_node = lake.build_queue.pushHead(t);
	INFO("Created target '", t->name(), "'.\n");
	return t;
}

/* ------------------------------------------------------------------------------------------------ lua__makeDep
 */
void lua__makeDep(Target* target, Target* prereq)
{
	INFO("Making '", prereq->name(), "' a prerequisite of target '", target->name(), "'.\n");
	SCOPED_INDENT;

	if (!target->prerequisites.has(prereq))
	{
		target->prerequisites.insert(prereq);
		target->unsatified_prereq_count += 1;
	}

	prereq->dependents.insert(target);

	if (target->build_node)
	{
		TRACE("Target is in build queue, so it will be removed\n");
		lake.build_queue.destroy(target->build_node);
		target->build_node = nullptr;
	}
}

/* ------------------------------------------------------------------------------------------------ lua__targetSetRecipe
 *  Sets the target as having a recipe and returns an index into the recipe table that the calling
 *  function is expected to use to set the recipe appropriately.
 */
s32 lua__targetSetRecipe(Target* target)
{
	lua_State* L = lake.L;
	lua_getglobal(L, lake_recipe_table);
	defer { lua_pop(L, 1); };

	if (target->flags.test(Target::Flags::HasRecipe))
	{
		WARN("Target '", target->name(), "'s recipe is being set again.\n");
		target->recipe_working_directory.destroy();
		target->recipe_working_directory = fs::Path::cwd();
		return target->lua_ref;
	}	
	else
	{
		TRACE("Target '", target->name(), "' is being marked as having a recipe.\n");
		target->flags.set(Target::Flags::HasRecipe);
		// TODO(sushi) targets that have their recipe set from the same directory should
		//             use the same path
		target->recipe_working_directory = fs::Path::cwd();

		// get the next slot to fill and then set 'next' to whatever that slot points to
		lua_pushlstring(L, "next", 4);
		lua_gettable(L, -2);

		s32 next = lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_pushlstring(L, "next", 4);
		lua_rawgeti(L, -2, next);

		if (lua_isnil(L, -1))
		{
			// if we werent pointing at another slot 
			// just set next to the next value
			lua_pop(L, 1);
			lua_pushnumber(L, next+1);
		}
		lua_settable(L, -3);

		target->lua_ref = next;
 
		return next;
	}
}

/* ------------------------------------------------------------------------------------------------ lua__createGroupTarget
 */
Target* lua__createGroupTarget()
{
	Target* group = lake.target_pool.add();
	group->init_group();
	group->build_node = lake.build_queue.pushHead(group);
	INFO("Created group '", (void*)group, "'.\n");
	return group;
}

/* ------------------------------------------------------------------------------------------------ lua__addTargetToGroup
 */
void lua__addTargetToGroup(Target* group, Target* target)
{
	assert_group(group);
	assert_single(target);

	INFO("Adding target '", target->name(), "' to group '", group->name(), "'.\n");
	SCOPED_INDENT;

	group->group.targets.insert(target);
	target->single.group = group;


	if (target->build_node)
	{
		TRACE("Target '", target->name(), "' is in the build queue so it will be removed.\n");
		lake.build_queue.remove(target->build_node);
		target->build_node = nullptr;
	}
}

/* ------------------------------------------------------------------------------------------------ lua__targetAlreadyInGroup
 */
b8 lua__targetAlreadyInGroup(Target* target)
{
	assert_single(target);
	return target->single.group != nullptr;
}


/* ------------------------------------------------------------------------------------------------ lua__getTargetPath
 */
str lua__getTargetPath(Target* target)
{
	assert_single(target);
	return target->name();
}

/* ------------------------------------------------------------------------------------------------ lua__nextCliarg
 */
const char* lua__nextCliarg()
{
	if (!lake.lua_cli_arg_iterator)
		return nullptr;

	const char* out = *lake.lua_cli_arg_iterator->data;
	lake.lua_cli_arg_iterator = lake.lua_cli_arg_iterator->next;
	return out;
}

/* ------------------------------------------------------------------------------------------------ lua__finalizeGroup
 *  Finalizes the given group. We assume no more targets will be added to it and hash together the
 *  hashes of each target in the group to form the hash of the group. 
 *
 *  TODO(sushi) implement this. I don't know if it is actually useful, its only usecase I can
 *              think of is referring to the same group by calling 'lake.targets' with the 
 *              same paths, but I don't intend on ever doing that myself.
 */
void lua__finalizeGroup(Target* target)
{
	assert_group(target);
}

/* ------------------------------------------------------------------------------------------------ lua__stackDump
 */
void lua__stackDump()
{
	stack_dump(lake.L);
}

/* ------------------------------------------------------------------------------------------------ lua__getMonotonicClock
 */
u64 lua__getMonotonicClock()
{
	auto now = TimePoint::monotonic();
	return now.s * 1e6 + now.ns / 1e3;
}

/* ------------------------------------------------------------------------------------------------ lua__makeDir
 */
b8 lua__makeDir(str path, b8 make_parents)
{
	return fs::Dir::make(path, make_parents);
}

/* ------------------------------------------------------------------------------------------------ lua__dirDestroy
 */
void lua__dirDestroy(fs::Dir* dir)
{
	mem::stl_allocator.deconstruct(dir);
}

/* ------------------------------------------------------------------------------------------------ lua__processSpawn
 */
ActiveProcess* lua__processSpawn(str* args, u32 args_count)
{
	assert(args && args_count);

	DEBUG("spawning process from file '", args[0], "'\n");
	SCOPED_INDENT;

	if (logger.verbosity == Logger::Verbosity::Trace)
	{
		TRACE("with args:\n");
		SCOPED_INDENT;
		for (s32 i = 0; i < args_count; i++)
		{
			TRACE(args[i], "\n");
		}
	}	

	ActiveProcess* proc = active_process_pool.add();
	Process::Stream streams[3] = {{}, {true, &proc->stdout}, {true, &proc->stderr}};
	proc->process = Process::spawn(args[0], {args+1, args_count-1}, streams, nil);
	if (isnil(proc->process))
	{
		ERROR("failed to spawn process using file '", args[0], "'\n");
		exit(1);
	}

	return proc;
}

/* ------------------------------------------------------------------------------------------------ lua__processRead
 */
void lua__processRead(
		ActiveProcess* proc, 
		void* stdout_ptr, u64 stdout_len, u64* out_stdout_bytes_read,
		void* stderr_ptr, u64 stderr_len, u64* out_stderr_bytes_read)
{
	assert(
		proc &&
		stdout_ptr && stdout_len && out_stdout_bytes_read &&
		stderr_ptr && stderr_len && out_stderr_bytes_read);
	TRACE("reading ", (void*)proc, "\n");

	*out_stdout_bytes_read = proc->stdout.read({(u8*)stdout_ptr, stdout_len});
	*out_stderr_bytes_read = proc->stderr.read({(u8*)stderr_ptr, stderr_len});
}

/* ------------------------------------------------------------------------------------------------ lua__processPoll
 */
b8 lua__processPoll(ActiveProcess* proc, s32* out_exit_code)
{
	assert(proc && out_exit_code);
	TRACE("polling ", (void*)proc, "\n");

	proc->process.checkStatus();

	if (proc->process.terminated)
	{
		*out_exit_code = proc->process.exit_code;
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------------------------------------ lua__processClose
 */
void lua__processClose(ActiveProcess* proc)
{
	assert(proc);
	TRACE("closing proc ", (void*)proc, "\n");

	proc->stdout.close();
	proc->stderr.close();
	active_process_pool.remove(proc);
}

/* ------------------------------------------------------------------------------------------------ lua__globCreate
 *  This could PROBABLY be implemented better but whaaatever.
 */
struct LuaGlobResult
{
	str* paths;
	s32 paths_count;
};

LuaGlobResult lua__globCreate(str pattern)
{

	auto matches = Array<str>::create();
	auto glob = fs::Globber::create(pattern);
	glob.run(
		[&matches](fs::Path& p)
		{
			str match = p.buffer.asStr();
			*matches.push() = match.allocateCopy();
			return true;
		});
	glob.destroy();
	return {matches.arr, matches.len()};
}

/* ------------------------------------------------------------------------------------------------ lua__globDestroy
 */
void lua__globDestroy(LuaGlobResult x)
{
	auto arr = Array<str>::fromOpaquePointer(x.paths);

	for (str& s : arr)
		mem::stl_allocator.free(s.bytes);

	arr.destroy();
}


/* ------------------------------------------------------------------------------------------------ lua__setMaxJobs
 */
void lua__setMaxJobs(s32 n)
{
	if (!lake.max_jobs_set_on_cli)
	{
		NOTICE("max_jobs set to ", n, " from lakefile call to lake.maxjobs\n");

		// TODO(sushi) make a thing to get number of logical processors and also 
		//             warn here if max jobs is set too high
		lake.max_jobs = n;
	}
}

/* ------------------------------------------------------------------------------------------------ lua__importFile
 */
int lua__importFile(lua_State* L)
{
	using Path = fs::Path;

	size_t len;
	const char* s = lua_tolstring(L, -1, &len);
	str path = {(u8*)s, len};

	io::Memory prog;
	prog.open();

	if (!lake.parseFile(path, &prog))
	{
		prog.close();
		return 0;
	}

	if (lake.print_transformed)
		printf("%.*s\n", prog.len, prog.buffer);

	auto cwd = Path::cwd();
	defer { cwd.chdir(); cwd.destroy(); };

	// so dumb 
	auto nullTerminated = Path::from(Path::removeBasename(path));
	defer { nullTerminated.destroy(); };

	if (!Path::chdir(nullTerminated.buffer.asStr()))
		return 0;

	int top = lua_gettop(L);

	INFO("Importing transformed lakefile from path '", path, "'\n"); 
	if (luaL_loadbuffer(L, (char*)prog.buffer, prog.len, s)) 
	{
		ERROR(lua_tostring(L, -1), "\n");
		lua_pop(L, 1);
		return 0;
	}

	lua_pushvalue(L, 1); // copy options table to top of stack so it can be used as an arg

	// call the imported file
	if (lua_pcall(L, 1, LUA_MULTRET, 0))
	{
		ERROR(lua_tostring(L, -1), "\n");
		lua_pop(L, 1);
		return 0;
	}

	// check how many thing the file returned
	int nresults = lua_gettop(L) - top;

	prog.close();

	// return all the stuff the file returned
	return nresults;
}

/* ------------------------------------------------------------------------------------------------ lua__cwd
 */
int lua__cwd(lua_State* L)
{
	auto cwd = fs::Path::cwd();
	defer { cwd.destroy(); };

	auto s = cwd.buffer.asStr();

	lua_pushlstring(L, (char*)s.bytes, s.len);

	return 1;
}

}
