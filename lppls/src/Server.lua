--- 
--- Implementation of the lsp server, in lua to simplify dealing with the 
--- lsp specification.
---

local log = require "Logger" ("lpp.lsp", Verbosity.Debug)
local json = require "JSON"
local util = require "Util"
local List = require "List"
local Type = require "Type"

local server = {}

--- LSP method handlers.
local handlers = {}

--- Information regarding the client we are connected to.
local client = {}

--- Unique value used to indicate the server should exit.
server.exit = {}

-- * --------------------------------------------------------------------------

local dumpValue = function(val, depth)
  log:info(util.dumpValueToString(val, depth))
end

-- * --------------------------------------------------------------------------

local notify = function(method, params)
  lpp_lsp_sendClientMessage(server.handle, json.encode
  {
    jsonrpc = "2.0",
    method = method,
    params = params,
  })
end

--- A text document with its content.
---
---@class TextDocumentItem
--- The document's URI.
---@field uri string
--- The language the document is written in.
---@field languageId string
--- The version of this document, which increases after each change.
---@field version number
--- Our version sent to sub servers.
---@field our_version number
--- The contents of this document.
---@field text string
--- A mapping from byte offsets in the text to lines.
---@field text_line_map table
--- The output of the preprocessed file, if we reached that point.
---@field output string?
--- A mapping from byte offsets in the output to lines.
---@field output_line_map table
--- The meta output of the preprocessed file, if we reached that point.
---@field meta string?
--- A mapping from byte offsets in the meta output to lines.
---@field meta_line_map table
--- Diagnostics found during preprocessing.
---@field diags List
--- Macro calls found during preprocessing.
---@field macro_calls List
local TextDocumentItem = Type.make()

TextDocumentItem.new = function(lsp_params)
  local o = lsp_params
  o.diags = List{}
  return setmetatable(o, TextDocumentItem)
end

local makeLineMap = function(text)
  local line_map = {}
  if text then
    local line = 1
    for start,stop in text:gmatch "().-()\n" do
      line_map[line] = {start, stop - start - 1}
      line = line + 1
    end
  end
  return line_map
end

TextDocumentItem.update = function(self)
  self.text_line_map = makeLineMap(self.text)
  self.output_line_map = makeLineMap(self.output)
  self.meta_line_map = makeLineMap(self.meta)
end

TextDocumentItem.emitDiags = function(self)
  local params = 
  {
    uri = self.uri,
    diagnostics = List{}
  }

  for diag in self.diags:each() do
    params.diagnostics:push
    {
      range = diag.range,
      message = diag.message,
      code = diag.code,
      source = diag.source,
      severity = diag.severity,
    }
  end

  notify("textDocument/publishDiagnostics", params)
end

--- Documents currently open on the client.
---
---@type table<string, TextDocumentItem>
local open_documents = {}

--- Documents mapped by their output name.
--- 
---@type table<string, TextDocumentItem>
local open_documents_by_output = {}

local semantic_legend = List
{
  "macro"
}

---@class SubServer
local SubServer = Type.make()

-- * --------------------------------------------------------------------------

---@return SubServer
SubServer.new = function(kind)
  local o = {}
  o.kind = kind
  o.id = 1
  return setmetatable(o, SubServer)
end

-- * --------------------------------------------------------------------------

SubServer.request = function(self, method, params)
  local id = self.id
  local request = 
  {
    jsonrpc = "2.0",
    id = id,
    method = method,
    params = params,
  }

  local encoded = json.encode(request)

  log:debug("sending request to ", self.kind, ":\n", encoded, "\n")

  lpp_lsp_sendServerMessage(server.handle, self.kind, encoded)
  self.id = id + 1
  return id
end

-- * --------------------------------------------------------------------------

SubServer.error = function(self, id, code, message)
  lpp_lsp_sendServerMessage(server.handle, self.kind, json.encode
  {
    jsonrpc = "2.0",
    id = id,
    code = code,
    message = message,
  })
end

-- * --------------------------------------------------------------------------

SubServer.notify = function(self, method, params)
  log:info("notify ", self.kind, " '", method, "': \n")
  local encoded = json.encode
  {
    jsonrpc = "2.0",
    method = method,
    params = params,
  }
  lpp_lsp_sendServerMessage(server.handle, self.kind, encoded)
end

-- * --------------------------------------------------------------------------

SubServer.poll = function(self)
  local response = lpp_lsp_readServerMessage(server.handle, self.kind)
  if not response then
    return
  end
  return json.decode(response)
end

local clangd = SubServer.new "clangd"

-- * --------------------------------------------------------------------------

handlers.initialize = function(params)
  log:debug("handling 'initialize'\n")

  client.name = params.clientInfo.name
  client.version = params.clientInfo.version
  client.processid = params.processId

  log:info("\nconnected to: ", 
           "\n   name ", client.name,
           "\nversion ", client.version,
           "\n procid ", client.processid, "\n\n")

  server.root_path = params.rootPath

  log:info("root path ", server.root_path, "\n")

  client.capabilities = params.capabilities

  -- Try opening a compile_commands.json to get more information on how 
  -- to compile files.
  
  local path = server.root_path.."/compile_commands.json"
  local ccjson = io.open(path, "r")
  if ccjson then
    -- Create a mapping from file name to the command and working dir.
    local decoded = json.decode(ccjson:read("*a"))
    server.compile_commands = {}
    for ent in decoded:each() do
      local uri = 'file://'..ent.directory..'/'..ent.file
      server.compile_commands[uri] = 
      {
        dir = ent.directory,
        cmd = ent.arguments
      }
    end
  end

  local id = clangd:request("initialize", 
  {
    processId = lpp_lsp_getPid(),
    rootUri = params.rootUri,
    rootPath = params.rootPath,
    clientInfo = 
    {
      name = "lppls",
      version = "0.1",
    },
    capabilities = 
    {
      workspace = 
      {
        didChangeWatchedFiles = 
        {
          dynamicRegistration = false
        },
        workspaceFolders = false
      },
      diagnostic = {}
    }
  })

  while true do
    local response = clangd:poll()
    if response then
      dumpValue(response, 3)
      
      if response.id == id then
        clangd.info = response.serverInfo
        clangd.capabilities = response.result.capabilities
        dumpValue(clangd.capabilities, 5)
        clangd:notify "initialized"
        break
      end
    end
  end

  return
  {
    serverInfo = 
    {
      name = "lpplsp",
      version = "0.1",
    },

    capabilities = 
    {
      positionEncoding = "utf-16",

      textDocumentSync = 
      {
        -- Recieve a notification when a document is opened/closed.
        openClose = true,

        -- Always sync by sending the full content of the document.
        change = 1,
      },

      -- Provide coloring support.
      -- colorProvider = true,

      -- Provide semantic tokens support.
      -- semanticTokensProvider = 
      -- {
      --   full = { delta = true },
      --   legend = semantic_legend,
      -- },

      diagnosticProvider = 
      {
        interFileDependencies = false
      },

      hoverProvider = true,
    }
  }
end

-- * --------------------------------------------------------------------------

handlers.initialized = function()
  -- Nothing to be done.
end

-- * --------------------------------------------------------------------------

local processDocument = function(document)
  local result = lpp_lsp_processFile(server.handle, document)

  document.output = result.output
  document.meta = result.meta

  if result then
    document.diags = List{}
    for _,diag in ipairs(result.diags) do
      document.diags:push(diag)
    end

    document.macro_calls = List{}
    for _,call in ipairs(result.macro_calls) do
      document.macro_calls:push(call)
    end

    document.expansions = {}
    document.expansions.meta = List(result.meta_expansions)
    document.expansions.output = List(result.output_expansions)
  end

  -- dumpValue(document, 5)
end

-- * --------------------------------------------------------------------------

handlers["textDocument/didOpen"] = function(params)
  if not params.textDocument.uri:match "^file://" then
    error "lpplsp only supports file:// uris"
  end

  local document = TextDocumentItem.new(params.textDocument)

  open_documents[document.uri] = document

  document.text = params.textDocument.text
  document.compile_command = server.compile_commands[document.uri]

  processDocument(document)
  
  if document.output_path then
    document.output_path = "file://"..document.output_path
    open_documents_by_output[document.output_path] = document
  end

  document:update()

  if document.output then
    clangd:notify("textDocument/didOpen",
    {
      textDocument = 
      {
        uri = document.output_path or document.uri,
        languageId = "cpp",
        version = 1,
        text = document.output,
      }
    })
  end

  document:emitDiags()
 end

-- * --------------------------------------------------------------------------

handlers["textDocument/didChange"] = function(params)
  local document = open_documents[params.textDocument.uri]

  if not document then
    log:error("recieved didChange for unopened document ", 
              params.textDocument.uri, "\n")
    return
  end

  document.text = params.contentChanges[1].text
  document.version = params.textDocument.version

  local old_output_line_map = document.output_line_map

  document:update()

  processDocument(document)

  if document.output then
    -- TODO(sushi) at some point we'll want to actually send didChange notifs
    --             to the document's ls, but I do not feel like figuring 
    --             out how to properly calculate that right now and with how
    --             lpp is at the moment I think it would even be less 
    --             efficient than just doing this.
    --             Due to the (relative) simplicity of what lpp is doing 
    --             vs an actual language, we could maybe get away with 
    --             setting up lpp to be capable of caching sections and 
    --             being able to perform partial edits of them followed
    --             by only recomputing what's necessary. Though, with how 
    --             macros are allow to touch other sections, that may not 
    --             work well. But for example, if we are notified that a
    --             Document section is what has changed, we could just 
    --             compute where in the output the change will have 
    --             occurred and update that. But macros make that difficult.
    clangd:notify("textDocument/didClose",
    {
      textDocument = 
      {
        uri = document.output_path or document.uri
      }
    })
    clangd:notify("textDocument/didOpen",
    {
      textDocument = 
      {
        uri = document.output_path or document.uri,
        languageId = "cpp",
        version = 1,
        text = document.output,
      }
    })
  end

  document:emitDiags()
end

-- * --------------------------------------------------------------------------

handlers["textDocument/semanticTokens/full"] = function(params)
  local uri = params.textDocument.uri
  
  local document = open_documents[uri]

  if not document then
    log:error("recieved semanticTokens/full for unopened document ", uri, "\n")
    return
  end

  local file = uri:gsub("file://", "")
end

-- * --------------------------------------------------------------------------

handlers["textDocument/hover"] = function(params)
  dumpValue(params, 3)

  local document = open_documents[params.textDocument.uri]
  if not document then
    error("textDocument/hover requested a file that is not opened '"..
          params.textDocument.uri.."'")
  end

  if not document.output then
    -- Can't show anything.
    return
  end

  local line_offsets = document.text_line_map[params.position.line+1]

  local cursor_pos = line_offsets[1] + params.position.character

  for call in document.macro_calls:each() do
    if call.macro_range[1] <= cursor_pos and 
       call.macro_range[2] >= cursor_pos 
    then
      return
      {
        contents = document.output:sub(call.expansion[1], call.expansion[2])
      }
    end
  end
end

-- * --------------------------------------------------------------------------

handlers["$/cancelRequest"] = function(params)
  return
  {
    code = -32601,
    message = "cancel"
  }
end

-- * --------------------------------------------------------------------------

server.setHandle = function(handle)
  server.handle = handle
end

-- * --------------------------------------------------------------------------

server.processMessage = function(msg)
  local decoded = json.decode(msg)

  log:info("recieved ", decoded.method, "\n")

  if decoded.method == "shutdown" then
    return server.exit
  end

  local handler = handlers[decoded.method]

  if not handler then
    return json.encode
    {
      jsonrpc = "2.0",
      id = decoded.id,
      code = -32601,
      message = "unhandled method "..decoded.method
    }
  end

  local result = handler(decoded.params)

  if decoded.id then
    local response = json.encode
    {
      jsonrpc = decoded.jsonrpc, 
      id = decoded.id,
      result = result,
    }
    
    return response
  end
end

local sub_handlers = {}

-- * --------------------------------------------------------------------------

sub_handlers["textDocument/publishDiagnostics"] = function(params)
  log:info("publishDiagnostics ", params.uri, "\n")
  local document = open_documents[params.uri]
  if not document then
    document = open_documents_by_output[params.uri]
    if not document then
      return
    end
  end

  for diag in params.diagnostics:each() do
    if diag.severity == 1 then
      for exp,i in document.expansions.output:eachWithIndex() do
        local next_exp = document.expansions.output[i + 1] 
        local next_exp_line = next_exp and next_exp.to.line or exp.to.line
        if exp.to.line <= diag.range.start.line and 
           next_exp_line >= diag.range.start.line  
        then
          local true_line = 
            exp.from.line + (diag.range.start.line - exp.to.line)
          local formDiag = function(line)
            document.diags:push
            {
              range = 
              {
                start = 
                {
                  line = line,
                  character = 0,
                },
                ["end"] = 
                {
                  line = line,
                  character = 0,
                }
              },
              message = diag.message,
              severity = diag.severity,
              code = diag.code,
              source = diag.source,
            }
          end
          formDiag(true_line)
          for _,macro_line in ipairs(exp.from.macro_stack) do
            formDiag(macro_line)
          end
          break
        end
      end
    end
  end

  document:emitDiags()
end

-- * --------------------------------------------------------------------------

server.pollSubServers = function()
  local result = {}

  local message = clangd:poll()

  while message do
    local handler = sub_handlers[message.method]
    if handler then
      handler(message.params)
    elseif message.id then
      clangd:error(message.id, -32601, "unhandled method")
    end
    message = clangd:poll()
  end

  return result
end

return server
