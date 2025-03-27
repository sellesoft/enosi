--- 
--- Implementation of the lsp server, in lua to simplify dealing with the 
--- lsp specification.
---

local log = require "Logger" ("lpp.lsp", Verbosity.Info)
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
  log:debug(util.dumpValueToString(val, depth))
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

--- A text document with its content.
---
---@class TextDocument : iro.Type
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
local TextDocument = Type.make()

---@return TextDocument
TextDocument.new = function(uri, text)
  local o = {}
  o.diags = List{}
  o.version = 1
  o.info_version = 0
  o.uri = uri
  o.text = text
  o.importers = {}
  return setmetatable(o, TextDocument)
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

TextDocument.update = function(self)
  self.text_line_map = makeLineMap(self.text)
  self.output_line_map = makeLineMap(self.output)
  self.meta_line_map = makeLineMap(self.meta)
end

TextDocument.emitDiags = function(self)
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

TextDocument.openOnDocumentServer = function(self)
  if self.clangd_open or not self.output then
    return
  end

  log:info("opening ", self.uri, " on document server\n")

  self.clangd_open = true

  clangd:notify("textDocument/didOpen",
    {
      textDocument = 
      {
        uri = self.output_path or self.uri,
        languageId = "cpp",
        version = 1,
        text = self.output,
      }
    })
end

TextDocument.closeOnDocumentServer = function(self)
  if not self.clangd_open then
    return
  end

  log:info("closing ", self.uri, " on document server\n")

  self.clangd_open = false

  clangd:notify("textDocument/didClose",
  {
    textDocument = 
    {
      uri = self.output_path or self.uri
    }
  })
end

--- Documents currently open on the client.
---
---@type table<string, TextDocument>
local documents = {}

--- Documents mapped by their output name.
--- 
---@type table<string, TextDocument>
local documents_by_output = {}


-- * --------------------------------------------------------------------------

local processDocument = function(tudoc, tgtdoc)
  log:debug("processing document ", tudoc.uri, "\n")
  local result = lpp_lsp_processFile(server.handle, tudoc, tgtdoc or false)

  tudoc.output = result.output

  for name,info in pairs(result.lex.docs) do
    local uri = "file://"..name

    local document = documents[uri]
    if not document then
      local f = io.open(name, "r")
      document = TextDocument.new(uri, f:read("*a"))
      documents[uri] = document
    end

    if document.info_version ~= document.version then
      log:debug("updating info for ", document.uri, "\n")
      
      document.diags = List(info.diags)

      document:update()
      document:emitDiags()
    end
  end

  for name,info in pairs(result.meta.docs) do
    local uri = "file://"..name

    local document = documents[uri]
    if not document then
      local f = io.open(name, "r")
      document = TextDocument.new(uri, f:read("*a"))
      documents[uri] = document
    end

    if document ~= tudoc then
      document.importers[tudoc.uri] = tudoc
      document.is_imported = true
    end

    if document.info_version ~= document.version then
      log:debug("updating info for ", document.uri, "\n")

      document.meta = info.output

      document.diags = List(info.diags)

      document.macro_calls = List{}
      for _,call in ipairs(info.macro_calls) do
        document.macro_calls:push(call)
      end

      document.expansions = {}
      document.expansions.meta = List(info.meta_expansions)
      document.expansions.output = List(info.output_expansions)

      document:update()
      document:openOnDocumentServer()
      document:emitDiags()

      document.info_version = document.version
    end
  end

  if tudoc.output and not tgtdoc then
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
    tudoc:closeOnDocumentServer()
    tudoc:openOnDocumentServer()
  end


  if tudoc.output_path and not tudoc.output_path:find "^file://" then
    tudoc.output_path = "file://"..tudoc.output_path
    documents_by_output[tudoc.output_path] = tudoc
  end
end

-- * --------------------------------------------------------------------------

local process_queue = List{}

local queueProcessDocument = function(tudoc, tgtdoc)
  for elem,i in process_queue:eachWithIndex() do
    if elem.tudoc == tudoc then
      process_queue:remove(i)
      break
    end
  end

  process_queue:pushFront
  {
    tgtdoc=tgtdoc,
    tudoc=tudoc
  }
end

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
      local path = ent.directory..'/'..ent.file
      local uri = 'file://'..path
      if uri:find "%.lpp$" then
        -- Eagerly process any .lpp file we find in order to support 
        -- showing diagnostics in .lh files. When we process a file we also
        -- keep track of whatever it imports, then those imported files 
        -- keep track of what imported them and when they are opened in the
        -- client, we process what imported that file rather than the file 
        -- itself such that we process it using proper arguments.
        -- TODO(sushi) we definitely want to move processing ALL of these 
        --             files out of initialization, but at the moment we 
        --             are totally synchronous. Once I get back to this 
        --             all of these 'jobs' should probably be queued up 
        --             and processed with a lower priority than files that
        --             are opened by the client.
        server.compile_commands[uri] = 
        {
          dir = ent.directory,
          cmd = ent.arguments
        }

        local f = io.open(path, "r")
        if f then
          local doc = 
            TextDocument.new(uri, f:read("*a"))
          doc.compile_command = server.compile_commands[uri]

          documents[uri] = doc

          queueProcessDocument(doc)
        else
          log:warn("found '", path, "' in compile_commands.json but the file",
                   "could not be open for read\n")
        end
      end
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
      if response.id == id then
        clangd.info = response.serverInfo
        clangd.capabilities = response.result.capabilities
        clangd:notify "initialized"
        break
      end
    end
  end

  log:info "initialized\n"

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
        interFileDependencies = true
      },

      -- TODO(sushi) reimplement when I feel like working on this again.
      -- hoverProvider = true,
    }
  }
end

-- * --------------------------------------------------------------------------

handlers.initialized = function()
  -- Nothing to be done.
end

-- * --------------------------------------------------------------------------

handlers["textDocument/didOpen"] = function(params)
  if not params.textDocument.uri:match "^file://" then
    error "lppls only supports file:// uris"
  end

  local document = documents[params.textDocument.uri]
  if not document then
    document = 
      TextDocument.new(
        params.textDocument.uri, 
        params.textDocument.text)
    documents[document.uri] = document
  end

  document.text = params.textDocument.text
  document.compile_command = server.compile_commands[document.uri]
  document.version = params.textDocument.version

  if document.is_imported then
    for _,importer in pairs(document.importers) do
      queueProcessDocument(importer, document)
      break
    end
  else
    queueProcessDocument(document)
  end
 end

-- * --------------------------------------------------------------------------

handlers["textDocument/didClose"] = function(params)
  if not params.textDocument.uri:match "^file://" then
    error "lppls only supports file:// uris"
  end

  local document = documents[params.textDocument.uri]
  if not document then
    document = 
      TextDocument.new(
        params.textDocument.uri, 
        params.textDocument.text)
    documents[document.uri] = document
  end

  document:closeOnDocumentServer()
end

-- * --------------------------------------------------------------------------

handlers["textDocument/didChange"] = function(params)
  local document = documents[params.textDocument.uri]

  if not document then
    log:error("recieved didChange for unopened document ", 
              params.textDocument.uri, "\n")
    return
  end

  document.text = params.contentChanges[1].text
  document.version = params.textDocument.version

  if document.is_imported then
    for _,importer in pairs(document.importers) do
      queueProcessDocument(importer, document)
      break
    end
  else
    queueProcessDocument(document)
  end
end

-- * --------------------------------------------------------------------------

-- TODO(sushi) handle someday. This would be more for getting semantic tokens
--             from the document's ls (currently hardcoded as clangd) rather
--             than coming up with some semantic tokens for lpp (though, 
--             we could, just don't see a reason to).
--             Also maybe eventually semantic tokens from luals, but getting
--             that to work properly was much harder than clangd so its 
--             been put off for now.
-- handlers["textDocument/semanticTokens/full"] = function(params)
--   local uri = params.textDocument.uri
--   
--   local document = documents[uri]
-- 
--   if not document then
--     log:error("recieved semanticTokens/full for unopened document ", uri, "\n")
--     return
--   end
-- 
--   local file = uri:gsub("file://", "")
-- end

-- * --------------------------------------------------------------------------

handlers["textDocument/hover"] = function(params)
  local document = documents[params.textDocument.uri]
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

  log:debug("recieved ", decoded.method, "\n")

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

-- * --------------------------------------------------------------------------

server.processFromQueue = function()
  local job = process_queue:popFront()
  if job then
    processDocument(job.tudoc, job.tgtdoc)
  end
end

local sub_handlers = {}

-- * --------------------------------------------------------------------------

sub_handlers["textDocument/publishDiagnostics"] = function(params)
  log:debug("publishDiagnostics ", params.uri, "\n")
  local document = documents[params.uri]
  if not document then
    document = documents_by_output[params.uri]
    if not document then
      return
    end
  end

  if not document.expansions then
    return
  end

  for diag in params.diagnostics:each() do
    if diag.severity == 1 then
      for exp,i in document.expansions.output:eachWithIndex() do
        local next_exp = document.expansions.output[i + 1] 
        local next_exp_line = 
          next_exp and next_exp.to.line or #document.output_line_map

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
