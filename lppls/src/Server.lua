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

--- A text document with its content.
---
---@class TextDocumentItem
--- The document's URI.
---@field uri string
--- The language the document is written in.
---@field languageId string
--- The version of this document, which increases after each change.
---@field version number
--- The contents of this document.
---@field text string
--- Diagnostics found during preprocessing.
---@field diags List
local TextDocumentItem = Type.make()

TextDocumentItem.new = function(lsp_params)
  local o = lsp_params
  o.diags = List{}
  return setmetatable(o, TextDocumentItem)
end

TextDocumentItem.addDiag = function(self, diag)
  
end

--- Documents currently open on the client.
---
---@type table<string, TextDocumentItem>
local open_documents = {}

local semantic_legend = List
{
  "macro"
}

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
      }
    }
  }
end

-- * --------------------------------------------------------------------------

handlers.initialized = function()
  -- Nothing to be done.
end

handlers["textDocument/didOpen"] = function(params)
  if not params.textDocument.uri:match "^file://" then
    error "lpplsp only supports file:// uris"
  end

  local document = TextDocumentItem.new(params.textDocument)

  open_documents[params.textDocument.uri] = document

  local result = 
    lpp_lsp_processFile(
      server.handle, document.uri, document.text)

  for i,diag in ipairs(result.diags) do
    document.diags:push(diag)
  end
end

-- * --------------------------------------------------------------------------

handlers["textDocument/didChange"] = function(params)
  local document = open_documents[params.textDocument.uri]

  if not document then
    log:error("recieved didChange for unopened document ", 
              params.textDocument.uri, "\n")
    return
  end

  document.version = params.textDocument.version
  document.text = params.contentChanges[1].text
  document.diags = List{}

  local result = 
    lpp_lsp_processFile(
      server.handle, document.uri, document.text)

  for i,diag in ipairs(result.diags) do
    document.diags:push(diag)
  end
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

handlers["textDocument/diagnostic"] = function(params)
  local doc = open_documents[params.textDocument.uri]
  if not doc then
    error("client requested diagnostics on unopened file "..
          params.TextDocumentItem.uri)
  end

  local result = 
  {
    kind = "full",
    items = List{}
  }

  for diag in doc.diags:each() do
    result.items:push
    {
      range = 
      { 
        start = 
        {
          line = diag.line - 1,
          character = 0,
        },
        ["end"] = 
        {
          line = diag.line - 1,
          character = 0
        }
      },
      message = diag.message
    }
  end

  log:info(require "Util" .dumpValueToString(result, 4))

  return result
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
  log:info("got handle: ", handle, "\n")
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
    log:error(util.dumpValueToString(decoded, 4), "\n\n")
    log:error("unhandled method '", decoded.method, "'\n")
    error()
  end

  local result = handler(decoded.params)

  if decoded.id then
    local response = json.encode
    {
      jsonrpc = decoded.jsonrpc, 
      id = decoded.id,
      result = result,
    }
    
    log:info(response, "\n")

    return response
  end
end

return server
