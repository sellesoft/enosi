#include "target.h"
#include "lake.h"

#include "stdlib.h"
#include "assert.h"

#include "iro/logger.h"

static Logger logger = 
  Logger::create("lake.target"_str, Logger::Verbosity::Notice);

/* ----------------------------------------------------------------------------
 */
u64 hashTarget(const Target* t)
{
  return t->hash;
}

/* ----------------------------------------------------------------------------
 */
void Target::initCommon()
{
  prerequisites.init();
  dependents.init();
  build_node = nullptr;
}

/* ----------------------------------------------------------------------------
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

/* ----------------------------------------------------------------------------
 */
void Target::initSingle(str path)
{
  initCommon();
  kind = Kind::Single;
  auto temp = fs::Path::from(path);
  mem::copy(&single.path, &temp, sizeof(fs::Path)); 
  // TODO(sushi) make a lexical absolute thing so that this can still work in 
  //             that case. This needs to be done because i use clang to get 
  //             the headers a cpp file depends on, which gives the paths 
  //             relative to where they're being included from, so if i dont 
  //             sanitize them multiple targets will be created for the same 
  //             file since targets are based on the path.
  if (single.path.exists())
    single.path.makeAbsolute();
  hash = path.hash();
}

/* ----------------------------------------------------------------------------
 */
void Target::initGroup()
{
  initCommon();
  kind = Kind::Group;
  hash = 0;
  group.targets = TargetSet::create();
}

/* ----------------------------------------------------------------------------
 */
void Target::deinit()
{
  prerequisites.deinit();
  dependents.deinit();
  if (kind == Kind::Group)
    group.targets.deinit();
  else
    single.path.destroy();
  recipe_working_directory.destroy();
}

/* ----------------------------------------------------------------------------
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

/* ----------------------------------------------------------------------------
 */
s64 Target::modtime()
{
  assert(kind != Kind::Unknown);
  switch (kind)
  {
    // TODO(sushi) what if our path is not null-terminated ?
    case Kind::Single: 
      return 
        fs::FileInfo::of(
          single.path.buffer.asStr())
            .last_modified_time.s;

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


/* ----------------------------------------------------------------------------
 */
b8 Target::isNewerThan(Target* t)
{
  return modtime() > t->modtime();
}

/* ----------------------------------------------------------------------------
 */
b8 Target::needsBuilt()
{
  switch (kind)
  {
    case Kind::Group:
    case Kind::Single: {
      if (flags.test(Flags::PrerequisiteJustBuilt))
      {
        TRACE("Target '", single.path, "' needs built because the "
              "'PrerequisiteJustBuilt' flag was set.\n");
        return true;
      }

      if (!exists())
      {
        TRACE("Target '", single.path, "' needs built because it does not "
              "exist on disk.\n");
        return true;
      }
    
      SCOPED_INDENT;
      for (Target& prereq : prerequisites)
      {
        if (prereq.isNewerThan(this))
        {
          TRACE("Target '", single.path, "' needs built because its "
                "prerequisite, '", prereq.name(), "', is newer.\n");
          return true;
        }
        TRACE("Prereq '", prereq.name(), "' is older than the target.\n");
      }

    } break;
  }

  return false;
}

/* ----------------------------------------------------------------------------
 */
b8 Target::hasRecipe()
{
  return flags.test(Flags::HasRecipe);
}

/* ----------------------------------------------------------------------------
 */
Target::RecipeResult Target::resumeRecipe(LuaState& lua)
{
  if (!lua_ref || !flags.test(Flags::HasRecipe))
  {
    ERROR("Target '", name(), "' had resume_recipe() called on it, but it "
          "has no recipe!");
    return RecipeResult::Error;
  }

  auto cwd = fs::Path::cwd(); // ugh man
  defer { cwd.chdir(); cwd.destroy(); };

  recipe_working_directory.chdir();

  lua.getglobal(lake_recipe_table);
  defer { lua.pop(); };

  lua.getglobal(lake_coroutine_resume);
  lua.rawgeti(-2, lua_ref);

  // Resume the recipe by calling co.resume(f).
  // We only ever expect 2 returns:
  // the first being co.resume reporting if everything went okay,
  // the second either being nil, or not nil. 
  // A recipe, for now, is expected to return nil when it is finished. 
  // Internally when we are asyncronously checking a process we return 'true' 
  // to indicate that the recipe is not yet finished, but we dont check for 
  // this explicitly.
  if (!lua.pcall(1, 2))
  {
    ERROR(lua.tostring(), "\n");
    return RecipeResult::Error;
  }

  b8 coroutine_success = lua.toboolean(-2);

  if (!coroutine_success)
  {
    // the second arg is the message given by whatever failure occured
    ERROR(lua.tostring(), "\n");
    return RecipeResult::Error;
  }

  // this kiiinda sucks a little but i want to support a recipe managing cwd, 
  // so just save the cwd everytime we resume. This is probably horribly 
  // inefficient and should be replaced with something that just reads cwd 
  // into the path obj already existing on the target
  recipe_working_directory.destroy();
  recipe_working_directory = fs::Path::cwd();

  defer { lua.pop(2); };

  if (lua.isnil())
    return RecipeResult::Finished;
  else
    return RecipeResult::InProgress;
}

/* ----------------------------------------------------------------------------
 */
void Target::updateDependents(TargetList& build_queue, b8 mark_just_built)
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
          INFO("Dependent '", dependent.name(), "' has no more unsatisfied "
               "prerequisites, adding it to the build queue.\n");
          dependent.build_node = build_queue.pushTail(&dependent);
          dependent.unsatified_prereq_count = 0;
        }
        else if (dependent.unsatified_prereq_count != 0)
        {
          
          dependent.unsatified_prereq_count -= 1;
          TRACE("Target '", name(), "' has ", 
                 dependent.unsatified_prereq_count, " unsatisfied prereqs "
                 "left.\n");
        }
      }
    } break;

    case Kind::Group: {
      for (auto& target : group.targets)
      {
        target.updateDependents(build_queue, mark_just_built);
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
