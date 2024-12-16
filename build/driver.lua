--- 
--- Parser for command line args and implementations of subcommands.
---

local sys = require "build.sys"
local List = require "list"
local bobj = require "build.object"

local driver = {}

driver.subcmds = {}

driver.run = function()
  local args = lake.cliargs

  if args:isEmpty() then
    driver.subcmds.build()
  else
    local subcmd = driver.subcmds[args[1]]
    if not subcmd then
      sys.log:fatal("unknown subcommand '",subcmd,"'")
      os.exit(1)
    end
    return (subcmd(args:sub(2)))
  end
end

driver.subcmds.build = function(args)
  for proj in List(sys.cfg.enabled_projects):each() do
    sys.getOrLoadProject(proj)
    -- Create the tasks.
    -- proj.task = lake.task(proj.name)
  end

  for name,proj in pairs(sys.projects) do
    print(name)
    local bobjs = List{}
    
    for BObj,list in pairs(proj.bobjs.private) do
      for bobj in list:each() do
        print("pri: ", bobj)
      end
    end

    for BObj,list in pairs(proj.bobjs.public) do
      for bobj in list:each() do
        print("pub: ", bobj)
      end
    end
  end
end

return driver
