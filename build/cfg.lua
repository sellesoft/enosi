--- 
--- Config loading.
---

local helpers = require "build.helpers"
local List = require "List"

local loadUserCfg = function(sys)
  local user_chunk, err = loadfile "user.lua"
  if not user_chunk then
    sys.log:fatal("failed to load user.lua: \n", err, "\n")
    os.exit(1)
  end

  local CfgEnv = {}
  setmetatable(CfgEnv, { __index=_G })

  CfgEnv.append = helpers.caller(function(list)
    return { is_append=true, list=list }
  end)

  setfenv(user_chunk, CfgEnv)

  sys.cfg = user_chunk()

  if not sys.cfg then
    sys.log:fatal("user.lua did not return a table")
    os.exit(1)
  end
end

local inheritOptions = function(sys, def)
  local opt_stack = List{}

  local function inherit(defv, usrv)
    for k,v in pairs(defv) do
      local uv = usrv[k]
      if not uv then
        usrv[k] = v
      else
        local ut = type(uv)
        local dt = type(v)
        if ut ~= dt then
          local opts = ""
          for opt,i in opt_stack:eachWithIndex() do
            opts = opts..opt
            if i ~= #opt_stack then
              opts = opts.."."
            end
          end
          sys.log:fatal(
            "user.lua specifies '",opts,"' as a ",ut," but it ",
            "should be a ",dt,"\n")
          os.exit(1)
        end
        if dt == "table" then
          opt_stack:push(k)
          inherit(v, uv)
          opt_stack:pop()
        end
      end
    end
  end

  inherit(def, sys.cfg)
end

return function(sys)

  local def = require "user_default"

  if not lake.pathExists "user.lua" then
    sys.cfg = def
  else
    loadUserCfg(sys)
  end

  -- Inherit options from default cfg.
  inheritOptions(sys, def)

  -- Define shorthands for accessing some config since know what 
  -- platform we are on. This might be silly.
  setmetatable(sys.cfg,
  {
    __index = function(_,k)
      local os_opt = rawget(sys.cfg, "os")[sys.os][k]
      return os_opt or rawget(sys.cfg, k)
    end
  })
end
