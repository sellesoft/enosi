--- 
--- The default user configuration for enosi's build system. This file can be
--- copied to user.lua to set personal build preferences. 
--- 
--- Note that this is always inherited into user.lua in order to ensure 
--- defaults are available, and it is inherited 'recursively'. Overwriting a
--- table value does not clear all of its children, you only overwrite the 
--- children you specify.
---

return 
{
  -- TODO(sushi) make an 'auto' option for this and have that be default.
  -- The maximum amount of build processes to allow running at the same time.
  max_jobs = 6,
  
  -- What projects we want to build. This expects there to be a directory
  --   'enosi/<proj-name>'
  -- containing a 'project.lua' file.
  enabled_projects = 
  {
    "lppclang",
    "iro",
    "lpp",
    "elua",
    "lake",
    --"hreload",
    --"experimental/eog",
    "ecs",
  },

  mode = "debug",

  -- OS specific config.
  os = 
  {
    linux = 
    {
      -- Processes to use when compiling C++.
      cpp = 
      {
        compiler = "clang++",
        linker = "ld",
      }
    },

    windows = 
    {
      cpp = 
      {
        compiler = "clang++",
        linker = "lld",
      }
    },
  },

  -- Attempt to enable address sanitizer. This doesn't appear to work well
  -- under Windows for now especially when using LLVM, so enabling this will
  -- likely break compilation. 
  -- TODO(sushi) get this working. I think I just need to adjust how llvm is 
  --             built or get it so that the build system is capable of 
  --             generating different build objects for prerequisite projects
  --             depending on the configuration of the project using it, which
  --             seems like way too complicated of an undertaking for now. Eg.
  --             trying to compile things using lppclang with this enabled will
  --             cause clang to trip use-after-poison errors. The only project 
  --             I actually want this for atm is ECS, but since enabling this 
  --             implies it is compiled into iro, which is used by both ECS 
  --             and lppclang, lppclang will also be using it which breaks 
  --             stuff.
  asan = false,

  -- Configuration intended to be applied to all projects.
  -- Note that any configuration specified in project specific 
  -- tables will override any that appear here.
  all_projects = 
  {
    enabled_warnings = {}
  }
}
