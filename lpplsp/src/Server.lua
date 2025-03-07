--- 
--- Implementation of the lsp server, in lua to simplify dealing with the 
--- lsp specification.
---

local log = require "Logger" ("lpp.lsp", Verbosity.Debug)
local json = require "JSON"
local util = require "Util"
local List = require "List"

local server = {}

--- LSP method handlers.
local handlers = {}

--- Information regarding the client we are connected to.
local client = {}

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
      colorProvider = true,

      -- Provide semantic tokens support.
      semanticTokensProvider = 
      {
        full = { delta = true },
        legend = semantic_legend,
      }
    }
  }
end

-- * --------------------------------------------------------------------------

handlers.initialized = function()
  -- Nothing to be done.
end

handlers["textDocument/didOpen"] = function(params)
  open_documents[params.textDocument.uri] = params.textDocument
end

-- * --------------------------------------------------------------------------

handlers["textDocument/didChange"] = function(params)
  log:info(util.dumpValueToString(params, 3), "\n")

  local document = open_documents[params.textDocument.uri]

  if not document then
    log:error("recieved didChange for unopened document ", 
              params.textDocument.uri, "\n")
    return
  end

  document.version = params.textDocument.version
  document.text = params.contentChanges[1].text
end

-- * --------------------------------------------------------------------------

handlers["textDocument/semanticTokens/full"] = function(params)
  log:info(util.dumpValueToString(params, 4), "\n")

  local uri = params.textDocument.uri
  
  local document = open_documents[uri]

  if not document then
    log:error("recieved semanticTokens/full for unopened document ", uri, "\n")
    return
  end

  local file = uri:gsub("file://", "")

  lpp_lsp_getSemanticTokens(self.handle, file, document.text)
end

-- * --------------------------------------------------------------------------

server.setHandle = function(handle)
  server.handle = handle
end

-- * --------------------------------------------------------------------------

server.processMessage = function(msg)
  local decoded = json.decode(msg)
  -- log:info(require "Util" .dumpValueToString(decoded, 4), "\n")

  local handler = handlers[decoded.method]

  if not handler then
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
