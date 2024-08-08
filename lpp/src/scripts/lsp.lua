-- Script for generating C types and de/serializers for those types
-- based on JSON schemas. Currently built specifically for lpp's language
-- server, but it may be useful to pull out and make more general later on.

-- TODO(sushi) remove
package.path = package.path..";./iro/iro/lua/?.lua"

local buffer = require "string.buffer"
local List = require "list"
local CGen = require "cgen"

---@class JSONSchemas
--- Mapping from type names to their definition
---@field map table
--- Ordered list of schemas as they are declared.
---@field list List

---@class json
---@field schemas JSONSchemas
local json = {}

json.schemas =
{
  map = {},
  list = List{},
}

local Schema,
      Object

setmetatable(json,
{
  __index = function(self, key)
    local handlers =
    {
      Schema = function()
        return function(name)
          return Schema.new(self, name)
        end
      end
    }

    local handler = handlers[key]
    if handler then
      return handler()
    end

    print(debug.traceback())
    error("no handler for key '"..key.."'")
  end
})

--- A json Schema, eg. a named Object that can be used to build other Scehma.
---@class Schema
--- The name of this Schema.
---@field name string
--- The object that defines the structure of this Schema.
---@field obj Object
Schema = {}
Schema.__index = Schema

Schema.__call = function(self, name)
  self.name = name
  self.json.user_schema.map[name] = self
  self.json.user_schema.list:push{name=name,def=self}
  return self.tbl
end

Schema.new = function(json)
  local o = {}
  o.json = json
  o.tbl = Table.new(nil,json)

  -- constructor for a new instance of this schema 
  o.new = function(tbl)
    return setmetatable(
    {
      tbl=tbl,
      ---@param c CGen
      toC = function(_, c, name)
        c:appendStructMember(o.name, name, "{}")
      end,

      ---@param c CGen
      writeDeserializer = function(_, c, name, obj, out)
        local vname = name.."Value"
        c:beginIf(
          "Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
        do
          c:beginIf(vname.."->kind != Value::Kind::Object")
          do
            c:append(
              "ERROR(\"unexpected type for value '"..name.."' expected a "..
              "table but got \", getValueKindString("..vname..
              "->kind), \"\\n\");")
            c:append("return false;")
          end
          c:endIf()

          c:beginIf(
            "!deserialize"..o.name.."(allocator, &"..vname.."->object,&"..
            out.."->"..name..")")
          do
            c:append("return false;")
          end
          c:endIf()
        end
        c:endIf()
      end,

      ---@param c CGen
      writeSerializer = function(_, c, name, obj, invar)
        newValue(c, name, "Object", obj, function(vname)
          c:beginIf(
            "!serialize"..o.name.."(json,&"..vname.."->object,"..invar.."."..
            name..", allocator)")
          do
            c:append('ERROR("failed to serialize ',invar,'.',name,'\\n");')
            c:append("return false;")
          end
          c:endIf()
        end)
      end
    }, o)
  end

  o.__call = function(self, name)
    self.tbl.member.list:push{name=name,type=self}
    self.tbl.member.map[name] = self
    return self.tbl
  end

  ---@param c CGen
  o.toCDef = function(self, c)
    c:beginStruct(self.name)
    self.tbl:toC(c)
    c:endStruct()
  end

  ---@param c CGen
  o.writeDeserializer = function(self, c)
    c:beginStaticFunction(
      "b8",
      "deserialize"..self.name, "mem::Allocator* allocator",
      "json::Object* root", self.name.."* out")
    do
      c:append("using namespace json;")

      self.tbl.member.list:each(function(member)
        if member.type.writeDeserializer then
          member.type:writeDeserializer(c, member.name, "root", "out")
        else
          print("missing deserializer for member "..member.name.."!")
        end
      end)
    end
    c:append("return true;")
    c:endFunction()
  end

  ---@param c CGen
  o.writeSerializer = function(self, c)
    c:beginStaticFunction(
      "b8",
      "serialize"..self.name, "json::JSON* json",
      "json::Object* inObj",
      self.name.."& in"..o.name,
      "mem::Allocator* allocator")
    do
      c:append("using namespace json;")

      self.tbl.member.list:each(function(member)
        if member.type.writeSerializer then
          member.type:writeSerializer(c, member.name, "inObj", "in"..o.name)
        else
          print("missing serializer for member "..member.name.."!")
        end
      end)
      c:append("return true;")
    end
    c:endFunction()
  end

  setmetatable(o,Schema)
  return o
end

local enumCall = function(self, name_or_elem)
  if not self.name then
    self.name = name_or_elem
    self.json.user_schema.map[name_or_elem] = self
    self.json.user_schema.list:push{name=name_or_elem,def=self}
    return self
  end

  -- otherwise collect elements
  self.elems:push(name_or_elem)

  return self
end

local enumNew = function(T, options)
  return function(tbl)
    return setmetatable(
    {
      ---@param c CGen
      toC = function(self, c, name)
        c:appendStructMember(T.name, name, "{}")
      end,
      writeDeserializer = options.writeDeserializer,
      writeSerializer = options.writeSerializer,
    },
    {
      __call = function(self, name)
        tbl.member.list:push{name=name,type=self}
        tbl.member.map[name] = self
        return tbl
      end,
      __index = function(self, key)
        if key == "Set" then
          local EnumSet = {}
          EnumSet.__index = EnumSet

          ---@param c CGen
          EnumSet.toC = function(self, c, name)
            c:appendStructMember(self.of.name.."Flags", name, "{}")
          end

          EnumSet.writeDeserializer = options.writeSetDeserializer
          EnumSet.writeSerializer = options.writeSetSerializer

          local s = setmetatable({of=T}, EnumSet)
          return function(name)
            tbl.member.list:push{name=name,type=s}
            tbl.member.map[name] = s
            return tbl
          end
        end
      end,
    })
  end
end

StringedEnum = {}
StringedEnum.new = function(json)
  local o = {}
  o.json = json
  o.elems = List{}

  local getFromStringName = function(name)
    return "get"..name.."FromString"
  end

  local getFromEnumName = function(name)
    return "getStringFrom"..name
  end

  ---@param c CGen
  o.toCDef = function(self, c)
    c:beginEnum(self.name)
    do
      self.elems:each(function(elem)
        for k in pairs(elem) do
          c:appendEnumElement(k)
        end
      end)
      c:appendEnumElement "COUNT"
    end
    c:endEnum()
    c:typedef("Flags<"..self.name..">", self.name.."Flags")

    local from_string_name = getFromStringName(o.name)
    c:beginStaticFunction(o.name, from_string_name, "str x")
    do
      c:beginSwitch("x.hash()")
      do
        o.elems:each(function(elem)
          for k,v in pairs(elem) do
            c:beginCase('"'..v..'"_hashed')
              c:append("return "..o.name.."::"..k..";")
            c:endCase()
          end
        end)
      end
      c:endSwitch()
      c:append(
        'assert(!"invalid string passed to ', from_string_name, '");')
      c:append("return {};")
    end
    c:endFunction()

    local from_enum_name = getFromEnumName(o.name)
    c:beginStaticFunction("str", from_enum_name, o.name.." x")
    do
      c:beginSwitch("x")
      do
        o.elems:each(function(elem)
          for k,v in pairs(elem) do
            c:beginCase(o.name.."::"..k)
              c:append("return \""..v.."\";")
            c:endCase()
          end
        end)
      end
      c:endSwitch()
      c:append(
        'assert(!"invalid value passed to ', from_enum_name, '");')
      c:append("return {};")
    end
    c:endFunction()
  end

  o.new = enumNew(o,
  {
    name = name,
    obj = Object.new(json),
  }

  json.schemas.map[name] = o
  json.schemas.list:push(o)

  return o.obj
end

--- Creates a new 'instance' of this schema to be 
Schema.newInstance = function()

end

---@class ObjectMembers
---@field list List
---@field map table

--- A json Object
---@class Object
--- A list and map of the members belonging to this Object.
---@field member ObjectMembers
--- The object this table exists in, if any.
---@field prev Object?
--- A reference to the json module.
---@field json json
--- If this Object instance has been added to 'prev' already.
---@field added boolean
Object = {}
Object.__index = function(self, key)
  local handlers = 
  {
    Object = function()
      return Object.new(self, self.json)
    end,
  }

  local handler = handlers[key]
  if handler then
    return handler()
  end

  
end

---@param prev Object?
---@param json json
---@return Object
Object.new = function(prev, json)
  ---@type Object
  local o =
  {
    json = json,
    prev = prev,
    added = false,
    member =
    {
      list = List{},
      map = {}
    }
  }
  return setmetatable(o, Object)
end

Object.__call = function(self, name)
  assert(not self.added, "this object was called twice")
  self.added = true
  if self.prev then
    self.prev.member.list:push{name=name,type=self}
    self.prev.member.map[name] = self
  end
  return self
end

Object.done = function(self)
  return self.prev
end

-- helper for client capability schemas that only define dynamicRegistration
local CCdynReg = function(name)
  return json.Schema(name.."ClientCapabilities")
    .Bool "dynamicRegistration"
end

json.StringedEnum "ResourceOperationKind"
  { Create = "create" }
  { Rename = "rename" }
  { Delete = "delete" }

json.StringedEnum "FailureHandlingKind"
  { Abort = "abort" }
  { Transactional = "transactional" }
  { TextOnlyTransactional = "textOnlyTransactional" }
  { Undo = "undo" }

json.StringedEnum "MarkupKind"
  { PlainText = "plaintext" }
  { Markdown = "markdown" }

json.NumericEnum "SymbolKind"
  "File"
  "Module"
  "Namespace"
  "Package"
  "Class"
  "Method"
  "Property"
  "Field"
  "Constructor"
  "Enum"
  "Interface"
  "Function"
  "Variable"
  "Constant"
  "String"
  "Number"
  "Boolean"
  "Array"
  "Object"
  "Key"
  "Null"
  "EnumMember"
  "Struct"
  "Event"
  "Operator"
  "TypeParameter"

json.NumericEnum "InsertTextMode"
  "AsIs"
  "AdjustIndentation"

json.NumericEnum "CompletionItemKind"
  "Text"
  "Method"
  "Function"
  "Constructor"
  "Field"
  "Variable"
  "Class"
  "Interface"
  "Module"
  "Property"
  "Unit"
  "Value"
  "Enum"
  "Keyword"
  "Snippet"
  "Color"
  "File"
  "Reference"
  "Folder"
  "EnumMember"
  "Constant"
  "Struct"
  "Event"
  "Operator"
  "TypeParameter"

CC "WorkspaceEdit"
  .Bool "documentChanges"
  .Bool "normalizesLineEndings"
  .Table "changeAnnotationSupport"
    .Bool "groupsOnLabel"
    :done()
  .ResourceOperationKind.Set "resourceOperations"
  .FailureHandlingKind "failureHandling"
  :done()

CCdynReg "DidChangeConfiguration"

CCdynReg "DidChangeWatchedFiles"
  .Bool "relativePathSupport"

CCdynReg "WorkspaceSymbol"
  .Table "symbolKind"
    .SymbolKind.Set "valueSet"

CCdynReg "ExecuteCommand"

local CCrefreshSupport = function(name)
  return json.Schema(name.."ClientCapabilities")
    .Bool "refreshSupport"
end

CCrefreshSupport "SemanticTokensWorkspace"
CCrefreshSupport "CodeLensWorkspace"
CCrefreshSupport "InlineValueWorkspace"
CCrefreshSupport "InlayHintWorkspace"
CCrefreshSupport "DiagnosticWorkspace"

CCdynReg "TextDocumentSync"
  .Bool "willSave"
  .Bool "willSaveWaitUntil"
  .Bool "didSave"

CCdynReg "Completion"
  .Bool "contextSupport"
  .Bool "insertTextMode"
  .Table "completionItem"
    .Bool "snippetSupport"
    .Bool "commitCharactersSupport"
    .Bool "deprecatedSupport"
    .Bool "preselectSupport"
    .Bool "insertReplaceSupport"
    .Bool "labelDetailsSupport"
    .MarkupKind.Set "documentationFormat"
    .Table "resolveSupport"
      .StringArray "properties"
      :done()
    .Table "insertTextModeSupport"
      .InsertTextMode.Set "valueSet"
      :done()
    :done()
  .Table "completionItemKind"
    .CompletionItemKind.Set "valueSet"
    :done()
  .Table "completionList"
    .StringArray "itemDefaults"
    :done()

CCdynReg "Hover"
  .MarkupKind.Set "contentFormat"

CCdynReg "SignatureHelp"
  .Table "signatureInformation"
    .MarkupKind.Set "documentationFormat"
    .Table "parameterInformation"
      .Bool "labelOffsetSupport"
      :done()
    .Bool "activeParameterSupport"
    :done()
  .Bool "contextSupport"

local CClinkSupport = function(name)
  return CCdynReg(name).Bool "linkSupport"
end

CClinkSupport "Declaration"
CClinkSupport "Definition"
CClinkSupport "TypeDefinition"
CClinkSupport "Implementation"

CCdynReg "Reference"
CCdynReg "DocumentHighlight"

CCdynReg "DocumentSymbol"
  .Bool "hierarchicalDocumentSymbolSupport"
  .Bool "labelSupport"
  .Table "symbolKind"
    .SymbolKind.Set "valueSet"
    :done()

json.StringedEnum "CodeActionKind"
  { Empty = "" }
  { QuickFix = "quickfix" }
  { Refactor = "refactor" }
  { RefactorExtract = "refactor.extract" }
  { RefactorInline = "refactor.inline" }
  { RefactorRewrite = "refactor.rewrite" }
  { Source = "source" }
  { SourceOrganizeImports = "source.organizeImports" }
  { SourceFixAll = "source.fixAll" }

CCdynReg "CodeAction"
  .Bool "isPreferredSupport"
  .Bool "disabledSupport"
  .Bool "dataSupport"
  .Bool "honorsChangeAnnotations"
  .Table "codeActionLiteralSupport"
    .Table "codeActionKind"
      .CodeActionKind.Set "valueSet"
      :done()
    :done()
  .Table "resolveSupport"
    .StringArray "properties"
    :done()

CCdynReg "CodeLens"

CCdynReg "DocumentLink"
  .Bool "tooltipSupport"

CCdynReg "DocumentColor"
CCdynReg "DocumentFormatting"
CCdynReg "DocumentRangeFormatting"
CCdynReg "DocumentOnTypeFormatting"

CCdynReg "Rename"
  .Bool "prepareSupport"
  .Bool "honorsChangeAnnotations"

json.NumericEnum "DiagnosticTag"
  "Unnecessary"
  "Deprecated"

CC "PublishDiagnostics"
  .Bool "relatedInformation"
  .Bool "versionSupport"
  .Bool "codeDescriptionSupport"
  .Bool "dataSupport"
  .Table "tagSupport"
    .DiagnosticTag.Set "valueSet"
    :done()

json.StringedEnum "FoldingRangeKind"
  { Comment = "comment" }
  { Imports = "imports" }
  { Region = "region" }

CCdynReg "FoldingRange"
  .UInt "rangeLimit"
  .Bool "lineFoldingOnly"
  .Table "foldingRangeKind"
    .FoldingRangeKind.Set "valueSet"
    :done()
  .Table "foldingRange"
    .Bool "collapsedText"
    :done()

CCdynReg "SelectionRange"
CCdynReg "LinkedEditingRange"
CCdynReg "CallHierarchy"

json.StringedEnum "TokenFormat"
  { Relative = "relative" }

CCdynReg "SemanticTokens"
  .Table "requests"
    .Union "range"
      .Bool
      .Table
        :done()
      :done()
    .Union "full"
      .Bool
      .Table
        .Bool "delta"
        :done()
      :done()
    :done()
  .StringArray "tokenTypes"
  .StringArray "tokenModifiers"
  .TokenFormat.Set "formats"
  .Bool "overlappingTokenSupport"
  .Bool "multilineTokenSupport"
  .Bool "serverCancelSupport"
  .Bool "augmentsSyntaxTokens"

CCdynReg "Moniker"
CCdynReg "TypeHierarchy"
CCdynReg "InlineValue"
CCdynReg "InlayHint"

CCdynReg "Diagnostic"
  .Bool "relatedDocumentSupport"

CC "TextDocument"
  .TextDocumentSyncClientCapabilities "synchronization"
  .CompletionClientCapabilities "completion"
  .HoverClientCapabilities "hover"
  .SignatureHelpClientCapabilities "signatureHelp"
  .DeclarationClientCapabilities "declaration"
  .DefinitionClientCapabilities "definition"
  .TypeDefinitionClientCapabilities "typeDefinition"
  .ImplementationClientCapabilities "implementation"
  .ReferenceClientCapabilities "references"
  .DocumentHighlightClientCapabilities "documentHighlight"
  .DocumentSymbolClientCapabilities "documentSymbol"
  .CodeActionClientCapabilities "codeAction"
  .CodeLensClientCapabilities "codeLens"
  .DocumentLinkClientCapabilities "documentLink"
  .DocumentColorClientCapabilities "colorProvider"
  .DocumentFormattingClientCapabilities "formatting"
  .DocumentRangeFormattingClientCapabilities "rangeFormatting"
  .DocumentOnTypeFormattingClientCapabilities "onTypeFormatting"
  .RenameClientCapabilities "rename"
  .PublishDiagnosticsClientCapabilities "publishDiagnostics"
  .FoldingRangeClientCapabilities "foldingRange"
  .SelectionRangeClientCapabilities "selectionRange"
  .LinkedEditingRangeClientCapabilities "linkedEditingRange"
  .CallHierarchyClientCapabilities "callHierarchy"
  .SemanticTokensClientCapabilities "semanticTokens"
  .MonikerClientCapabilities "moniker"
  .TypeHierarchyClientCapabilities "typeHierarchy"
  .InlineValueClientCapabilities "inlineValue"
  .InlayHintClientCapabilities "inlayHint"
  .DiagnosticClientCapabilities "diagnostic"

CC "ShowMessageRequest"
  .Table "messageActionItem"
    .Bool "additionalPropertiesSupport"
    :done()

CC "ShowDocument"
  .Bool "support"

CC "RegularExpressions"
  .String "engine"
  .String "version"

CC "Markdown"
  .String "parser"
  .String "version"
  .StringArray "allowedTags"

json.StringedEnum "PositionEncodingKind"
  { UTF8  = "utf-8"  }
  { UTF16 = "utf-16" }
  { UTF32 = "utf-32" }

json.Schema "ClientCapabilities"
  .Table "workspace"
    .Bool "applyEdit"
    .Bool "workspaceFolders"
    .Bool "configuration"
    .Table "fileOperations"
      .Bool "dynamicRegistration"
      .Bool "didCreate"
      .Bool "willCreate"
      .Bool "didRename"
      .Bool "willRename"
      .Bool "didDelete"
      .Bool "willDelete"
      :done()
    .WorkspaceEditClientCapabilities "workspaceEdit"
    .DidChangeConfigurationClientCapabilities "didChangeConfiguration"
    .DidChangeWatchedFilesClientCapabilities "didChangeWatchedFiles"
    .WorkspaceSymbolClientCapabilities "symbol"
    .ExecuteCommandClientCapabilities "executeCommand"
    .SemanticTokensWorkspaceClientCapabilities "semanticTokens"
    .CodeLensWorkspaceClientCapabilities "codeLens"
    .InlineValueWorkspaceClientCapabilities "inlineValue"
    .InlayHintWorkspaceClientCapabilities "inlayHint"
    .DiagnosticWorkspaceClientCapabilities "diagnostics"
    :done()
  .Table "window"
    .Bool "workDoneProgress"
    .ShowMessageRequestClientCapabilities "showMessage"
    .ShowDocumentClientCapabilities "showDocument"
    :done()
  .Table "general"
    .Table "staleRequestSupport"
      .Bool "cancal"
      .StringArray "retryOnContentModified"
      :done()
    .RegularExpressionsClientCapabilities "regularExpressions"
    .MarkdownClientCapabilities "markdown"
    .PositionEncodingKind.Set "positionEncodings"
    :done()
  .TextDocumentClientCapabilities "textDocument"

json.Schema "InitializeParams"
  .Int "processId"
  .Table "clientInfo"
    .String "name"
    .String "version"
    :done()
  .String "locale"
  .String "rootUri"
  .String "rootPath"
  .ClientCapabilities "capabilities"
  :done()

json.NumericEnum "TextDocumentSyncKind"
  "None"
  "Full"
  "Incremental"

json.Schema "CompletionOptions"
  .StringArray "triggerCharacters"
  .StringArray "allCommitCharacters"
  .Bool "resolveProvider"
  .Table "completionItem"
    .Bool "labelDetailsSupport"
    :done()

json.Schema "SignatureHelpOptions"
  .StringArray "triggerCharacters"
  .StringArray "retriggerCharacters"

json.Schema "CodeLensOptions"
  .Bool "resolveProvider"

json.Schema "DocumentLinkOptions"
  .Bool "resolveProvider"

json.Schema "DocumentOnTypeFormattingOptions"
  .String "firstTriggerCharacter"
  .StringArray "moreTriggerCharacter"

json.Schema "ExecuteCommandOptions"
  .StringArray "commands"

json.Schema "SemanticTokensLegend"
  .StringArray "tokenTypes"
  .StringArray "tokenModifiers"

json.Schema "SemanticTokensOptions"
  .SemanticTokensLegend "legend"
  .Union "range"
    .Bool
    .Table
      :done()
    :done()
  .Union "full"
    .Bool
    .Table
      .Bool "delta"
      :done()
    :done()

json.Schema "DiagnosticOptions"
  .String "identifier"
  .Bool "interFileDependencies"
  .Bool "workspaceDiagnostics"

json.Schema "WorksapceFoldersServerCapabilities"
  .Bool "supported"
  .Union "changeNotifications"
    .String
    .Bool
    :done()

json.StringedEnum "FileOperationPatternKind"
  { File = "file" }
  { Folder = "folder" }

json.Schema "FileOperationPatternOptions"
  .Bool "ignoreCase"

json.Schema "FileOperationPattern"
  .String "glob"
  .FileOperationPatternKind "matches"
  .FileOperationPatternOptions "options"

json.Schema "FileOperationFilter"
  .String "scheme"
  .FileOperationPattern "pattern"

json.Schema "FileOperationRegistrationOptions"
  .SchemaArray "FileOperationFilter" "filters"

json.Schema "ServerCapabilities"
  .PositionEncodingKind "positionEncoding"
  .TextDocumentSyncKind "textDocumentSync"
  .CompletionOptions "completionProvider"
  .SignatureHelpOptions "signatureHelpProvider"
  .CodeLensOptions "codeLensProvider"
  .DocumentLinkOptions "documentLinkProvider"
  .DocumentOnTypeFormattingOptions "documentOnTypeFormattingProvider"
  .ExecuteCommandOptions "executeCommandProvider"
  .SemanticTokensOptions "semanticTokensProvider"
  .DiagnosticOptions "diagnosticProvider"
  .Bool "hoverProvider"
  .Bool "declarationProvider"
  .Bool "definitionProvider"
  .Bool "typeDefinitionProvider"
  .Bool "implementationProvider"
  .Bool "referencesProvider"
  .Bool "documentHighlightProvider"
  .Bool "documentSymbolProvider"
  .Bool "codeActionProvider"
  .Bool "colorProvider"
  .Bool "documentFormattingProvider"
  .Bool "documentRangeFormattingProvider"
  .Bool "renameProvider"
  .Bool "foldingRangeProvider"
  .Bool "selectionRangeProvider"
  .Bool "linkedEditingRangeProvider"
  .Bool "monikerProvider"
  .Bool "typeHierarchyProvider"
  .Bool "inlineValueProvider"
  .Bool "inlayHintProvider"
  .Bool "workspaceSymbolProvider"
  .Table "workspace"
    .WorksapceFoldersServerCapabilities "workspaceFolders"
    .Table "fileOperations"
      .FileOperationRegistrationOptions "didCreate"
      .FileOperationRegistrationOptions "willCreate"
      .FileOperationRegistrationOptions "didRename"
      .FileOperationRegistrationOptions "willRename"
      .FileOperationRegistrationOptions "didDelete"
      .FileOperationRegistrationOptions "willDelete"
      :done()
    :done()

local c = CGen.new()
json.user_schema.list:each(function(schema)
  schema.def:toCDef(c)
  if schema.def.writeDeserializer then
    schema.def:writeDeserializer(c)
    schema.def:writeSerializer(c)
  end
end)
print(c.buffer)
