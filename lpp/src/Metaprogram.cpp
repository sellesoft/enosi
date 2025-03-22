#include "Metaprogram.h"
#include "Lpp.h"

#include "iro/LineMap.h"
#include "iro/fs/File.h"
#include "iro/Platform.h"

#include "cstdlib"

namespace lpp
{

static Logger logger = 
  Logger::create("lpp.metaprog"_str, Logger::Verbosity::Notice);

/* ----------------------------------------------------------------------------
 */
b8 Section::initDocument(
    u64 token_idx,
    String raw, 
    SectionNode* node,
    io::Memory* buffer)
{
  assert(buffer);
  this->buffer = buffer;
  this->node = node;
  this->token_idx = token_idx;
  if (!buffer->open(raw.len))
    return false;
  buffer->write(raw);
  kind = Kind::Document;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Section::initDocumentSpan(
    u64 token_idx,
    SectionNode* node)
{
  this->node = node;
  this->token_idx = token_idx;
  kind = Kind::DocumentSpan;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Section::initMacro(
    u64 token_idx, 
    u64 macro_idx, 
    SectionNode* node)
{
  this->token_idx = token_idx;
  this->node = node;
  this->macro_idx = macro_idx;
  kind = Kind::Macro;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Section::initMacroImmediate(u64 token_idx, SectionNode* node)
{
  this->token_idx = token_idx;
  this->node = node;
  kind = Kind::MacroImmediate;
  return true;
}

/* ----------------------------------------------------------------------------
 */
void Section::deinit()
{
  switch (kind)
  {
  case Kind::Invalid:
    return;

  case Kind::Document:
    buffer = nullptr;
    break;
  }

  node = nullptr;
  kind = Kind::Invalid;
}

/* ----------------------------------------------------------------------------
 */
b8 Section::insertString(u64 offset, String s)
{
  assert(offset <= buffer->len);

  buffer->reserve(s.len);

  mem::move(
    buffer->ptr + offset + s.len, 
    buffer->ptr + offset, 
    (buffer->len - offset) + 1);
  mem::copy(buffer->ptr + offset, s.ptr, s.len);

  buffer->commit(s.len);

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Section::consumeFromBeginning(u64 len)
{
  if (len > buffer->len)
    return false;

  if (len == buffer->len)
  {
    buffer->clear();
    return true;
  }

  // idk man find a way to not move shit around later maybe with just a String ? 
  // idk we edit stuff too much and IDRC at the moment!!!
  mem::move(buffer->ptr, buffer->ptr + len, buffer->len - len);
  buffer->len -= len;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Metaprogram::init(
    Lpp* lpp, 
    io::IO* instream, 
    Source* input, 
    Source* output,
    Metaprogram* prev)
{
  this->lpp = lpp;
  this->instream = instream;
  this->input = input;
  this->output = output;
  this->prev = prev;
  current_section = nullptr;
  if (!buffers.init()) return false;
  if (!sections.init()) return false;
  if (!scope_stack.init()) return false;
  if (!expansions.init()) return false;
  if (!captures.init()) return false;
  if (!meta.init("meta"_str)) return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::deinit()
{
  for (auto& section : sections)
    section.deinit();
  sections.deinit();
  for (auto& scope : scope_stack)
    scope.deinit();
  scope_stack.deinit();
  expansions.deinit();
  for (auto& buffer : buffers)
    buffer.close();
  buffers.deinit();
  parser.deinit();
  captures.destroy();
  meta.deinit();
  *this = {};
}

/* ----------------------------------------------------------------------------
 */
b8 Scope::init(Scope* prev, io::Memory* buffer, Section* macro_invocation)
{
  this->prev = prev;
  this->buffer = buffer;
  this->macro_invocation = macro_invocation;
  sections = SectionList::create();

  if (prev)
    global_offset = prev->global_offset + prev->buffer->len;

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Scope::deinit()
{
  sections.deinit();
  *this = {};
}

/* ----------------------------------------------------------------------------
 */
Scope* Metaprogram::pushScope()
{
  auto* current = scope_stack.headNode();
  auto* node = scope_stack.push();
  auto* scope = node->data;

  auto* buffer = buffers.push()->data;
  if (!buffer->open())
    return nullptr;
  
  Section* macro_invocation = nullptr;
  if (current_section && current_section->data->kind == Section::Kind::Macro)
    macro_invocation = current_section->data;

  if (!scope->init(
      (current? current->data : nullptr), 
      buffer, 
      macro_invocation))
    return nullptr;

  return scope;
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::popScope()
{
  auto scope = scope_stack.pop();
  scope.deinit();
}

/* ----------------------------------------------------------------------------
 */
Scope* Metaprogram::getCurrentScope()
{
  return (scope_stack.isEmpty()? nullptr : &scope_stack.head());
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::addDocumentSection(u64 start, String raw)
{
  auto node = getCurrentScope()->sections.pushTail();
  node->data = sections.pushTail()->data;
  node->data->initDocument(start, raw, node, buffers.push()->data);
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::addDocumentSpanSection(u64 token_idx)
{
  auto node = getCurrentScope()->sections.pushTail();
  node->data = sections.pushTail()->data;
  node->data->initDocumentSpan(token_idx, node);
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::addMacroSection(s64 start, u64 macro_idx)
{
  auto node = getCurrentScope()->sections.pushTail();
  node->data = sections.pushTail()->data;
  node->data->initMacro(start, macro_idx, node);
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::addMacroImmediateSection(u64 start)
{
  auto node = getCurrentScope()->sections.pushTail();
  node->data = sections.pushTail()->data;
  node->data->initMacroImmediate(start, node);
}

/* ----------------------------------------------------------------------------
 */
static void printExpansion(Source* input, Source* output, Expansion& expansion)
{
  input->cacheLineOffsets();
  output->cacheLineOffsets();

  Source::Loc old = input->getLoc(expansion.from);
  Source::Loc nu  = output->getLoc(expansion.to);
  INFO(
     old.line, ":", old.column, "(", expansion.from, ") -> ", 
     nu.line, ":", nu.column, "(", expansion.to, ")\n");
}

/* ----------------------------------------------------------------------------
 *  Translates a lua error string to the original location in the input.
 *  This is probably only going to be used for syntax errors that occur
 *  when loading the metaprogram into lua.
 *
 *  This doesn't handle all line locations. Unfortunately in a case like 
 *  missing an 'end' lua will report the start of the end in the message.
 *  If we want to handle a case like this we would need to modify the 
 *  message or make a new one which I don't want to get into rn.
 *
 *  TODO(sushi) handle this better.
 *  TODO(sushi) SmallArray might be useful here.
 */
struct TranslatedError
{
  String filename;
  u64 line;
  String error;
};

TranslatedError translateLuaError(Metaprogram& mp, String error)
{
  TranslatedError te = {nil};

  if (error.startsWith("[string"_str))
  {
    error = error.sub("[string "_str.len + 1);
    te.filename = error.sub(0, error.findFirst('"'));
    error = error.sub(te.filename.len + 3);
    String num = error.sub(0, error.findFirst(':'));
    u8 terminated[24];
    if (!num.nullTerminate(terminated, 24))
    {
      ERROR(
        "unable to null terminate lua error line number, buffer too small");
      return {};
    }
    te.line = strtoull((char*)terminated, nullptr, 10);
    te.line = mp.mapMetaprogramLineToInputLine(te.line);
    te.error = error.sub(num.len + 2);
  }
  else
  {
    ERROR("unhandled lua error format: \n", error, "\n");
    return {};
  }

  return te;
}

/* ----------------------------------------------------------------------------
 */
Metaprogram::StackEnt Metaprogram::getStackEnt(s32 stack_idx, s32 ent_idx)
{
  StackEnt ent = {};

  auto& lua = lpp->lua;

  lua.pushinteger(ent_idx);
  lua.gettable(stack_idx);
  const s32 I_ent = lua.gettop();

  lua.pushstring("src"_str);
  lua.gettable(I_ent);
  ent.src = lua.tostring();

  lua.pop();

  lua.pushstring("line"_str);
  lua.gettable(I_ent);
  ent.line = lua.tonumber();
  lua.pop();

  lua.pushstring("name"_str);
  lua.gettable(I_ent);
  ent.name = lua.tostring();
  lua.pop();

  lua.pushstring("metaenv"_str);
  lua.gettable(I_ent);
  if (!lua.isnil())
  {
    lua.pushstring("ctx"_str);
    lua.gettable(-2);
    ent.mp = lua.tolightuserdata<Metaprogram>();
    lua.pop();
  }
  lua.pop();

  return ent;
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::emitError(s32 errinfo_idx, Scope* scope)
{
  auto& lua = lpp->lua;

  lua.pushstring("msg"_str);
  lua.gettable(errinfo_idx);
  const s32 I_msg = lua.gettop();

  lua.pushstring("stack"_str);
  lua.gettable(errinfo_idx);
  const s32 I_stack = lua.gettop();

  String message = lua.tostring(I_msg);

  s32 stack_len = lua.objlen();

  if (lpp->consumers.meta)
  {
    for (s32 i = 1; i <= stack_len; ++i)
    {
      StackEnt ent = getStackEnt(I_stack, i);

      if (ent.mp)
      {
        MetaprogramDiagnostic diag = {};
        diag.source = ent.mp->input;
        diag.loc = ent.mp->mapMetaprogramLineToInputLine(ent.line);
        diag.message = message;
        lpp->consumers.meta->consumeDiag(*this, diag);
      }
    }
  }
  else
  {
    for (s32 i = 1; i <= stack_len; ++i)
    {
      StackEnt ent = getStackEnt(I_stack, i);

      if (ent.mp)
        ent.line = ent.mp->mapMetaprogramLineToInputLine(ent.line);

      ERROR(ent.src, ":", ent.line, ": ");

      if (notnil(ent.name))
        ERROR("in ", ent.name, ":");

      ERROR("\n");
    }
  }

  while (scope)
  {
    if (scope->macro_invocation)
    {
      lua.pushinteger(scope->macro_invocation->macro_idx);
      lua.gettable(I.macro_names);

      u64 start_offset = 
        parser.lexer.tokens[scope->macro_invocation->token_idx].loc;

      Source::Loc loc = input->getLoc(start_offset);

      lua.pushinteger(scope->macro_invocation->macro_idx);
      lua.gettable(I.macro_invocations);
      const s32 I_stack = lua.gettop();

      s32 stack_len = lua.objlen();
      
      if (lpp->consumers.meta)
      {
        for (s32 i = 1; i <= stack_len; ++i)
        {
          StackEnt ent = getStackEnt(I_stack, i);

          if (ent.mp)
          {
            MetaprogramDiagnostic diag = {};
            diag.source = ent.mp->input;
            diag.loc = loc.line;
            diag.message = message;
            lpp->consumers.meta->consumeDiag(*this, diag);
          }
        }
      }
      else
      {
        for (s32 i = 1; i <= stack_len; ++i)
        {
          StackEnt ent = getStackEnt(I_stack, i);

          if (ent.mp)
            ent.line = ent.mp->mapMetaprogramLineToInputLine(ent.line);

          ERROR(ent.src, ":", loc.line, ": ");

          if (notnil(ent.name))
            ERROR("in ", ent.name, ":");

          ERROR("\n");
        }
      }

      // ERROR(message, "\n");
      lua.pop();
    }

    scope = scope->prev;
  }

  ERROR(lua.tostring(I_msg), "\n");
}

/* ----------------------------------------------------------------------------
 */
b8 Metaprogram::run()
{
  using enum Section::Kind;

  LuaState& lua = lpp->lua;

  const s32 bottom = lua.gettop();
  defer { lua.settop(bottom); };

  if (!lua.require("Errh"_str))
  {
    ERROR("failed to load errhandler module");
    return false;
  }
  I.errhandler = lua.gettop();

  // ~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~ Phase 1
  
  DEBUG("phase 1\n");

  // Parse into the lua 'metacode' that we run in Phase 2 to form the sections 
  // processed by Phase 3.

  if (!parser.init(
        input, 
        instream, 
        &meta.cache, 
        lpp->consumers.lex_diag_consumer))
    return false;
  defer { parser.deinit(); };

  if (setjmp(parser.err_handler))
    return false;

  if (!parser.run())
    return false;

  meta.cacheLineOffsets();

  for (auto& exp : parser.locmap)
  {
    DEBUG(exp.from, " -> ", exp.to, "\n");
  }

  // NOTE(sushi) we only want to output the main lpp file's metafile
  if (lpp->streams.meta.io  && prev == nullptr)
    io::format(lpp->streams.meta.io, meta.cache.asStr());

  if (lpp->consumers.meta)
    lpp->consumers.meta->consumeMetafile(*this, meta.cache.asStr());

  // ~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~ Phase 2
  
  DEBUG("phase 2\n");

  // Execute the lua metacode to form the sections we process in the following 
  // phase.

  TRACE("creating global scope\n");
  pushScope();
  defer { popScope(); };

  TRACE("loading parsed program\n");
  if (!lua.loadbuffer(meta.cache.asStr(), (char*)input->name.ptr))
  {
    auto te = translateLuaError(*this, lua.tostring());
    if (lpp->consumers.meta)
    {
      MetaprogramDiagnostic diag;
      diag.source = input;
      diag.loc = te.line;
      diag.message = te.error;
      lpp->consumers.meta->consumeDiag(*this, diag);
    }
    else
    {
      ERROR(
          "failed to load metaprogram into lua\n",
          te.filename, ":", te.line, ": ", te.error, "\n");
    }

    return false;
  }
  I.metaprogram = lua.gettop();

  // Get the lpp module and set the proper metaprogram context. 
  // TODO(sushi) this should already be on the stack from lpp's initialization
  //             and then used here.
  TRACE("setting lpp module context and metaenv\n");
  if (!lua.require("Lpp"_str))
  {
    ERROR("failed to get lpp module for setting metaprogram context\n");
    return false;
  }
  I.lpp = lua.gettop();

  // Save the old context.
  lua.pushstring("context"_str);
  lua.gettable(I.lpp);
  I.prev_context = lua.gettop();

  // Set the new context.
  lua.pushstring("context"_str);
  lua.pushlightuserdata(this);
  lua.settable(I.lpp);

  // Ensure the context is restored.
  defer 
  {
    lua.pushstring("context"_str);
    lua.pushvalue(I.prev_context);
    lua.settable(I.lpp);
  };

  // Save the old metaenv.
  lua.pushstring("metaenv"_str);
  lua.gettable(I.lpp);
  I.prev_metaenv = lua.gettop();

  // Ensure the metaenv is restored.
  defer
  {
    lua.pushstring("metaenv"_str);
    lua.pushvalue(I.prev_metaenv);
    lua.settable(I.lpp);
  };

  // Get the metaenvironment construction function and call it to 
  // make a new env.
  TRACE("constructing metaenvironment\n");
  if (!lua.require("Metaenv"_str))
  {
    ERROR("failed to load metaenvironment module\n");
    return false;
  }

  // Call the construction function with this Metaprogram.
  lua.pushlightuserdata(this);
  if (!lua.pcall(1, 1))
  {
    ERROR("metaenvironment construction function failed\n",
          lua.tostring(), "\n");
    return false;
  }
  I.metaenv = lua.gettop();

  // Get the ACTUAL metaenvironment table.
  lua.pushstring("__metaenv"_str);
  lua.gettable(I.metaenv);
  I.metaenv__metaenv = lua.gettop();

  // Get and set the new metaenv on lpp.
  lua.pushstring("metaenv"_str);
  lua.pushvalue(I.metaenv__metaenv);
  lua.settable(I.lpp);

  defer
  {
    // Mark the metaenvironment as having exited when we're done.
    // This helps prevent using metaenvironments from other lpp files as this 
    // is not currently supported. This can occur when two lpp files 
    // require the same lua module and one patches in a function that 
    // uses doc, val, or macro functions in its metaprogram then the other
    // calls that function. The metaenvironment/program at this point is
    // no longer valid and stuff like erroring and placing sections breaks.
    // In order to support this we would probably have to move back to 
    // using a single metaenvironment again which introduces some 
    // complications. Each metaenvironment stores a reference to the 
    // metaprogram that created it and so when one of its doc, val, or macro
    // functions is called in this way it will attempt to perform the logic
    // on the metaprogram that no longer exists. This can be fixed in some 
    // way but I don't feel a need to yet.
    // This might also break if the metaenv is garbage collected by lua.
    // Idk yet if it will be but we'll see :3.
    // Ideally this SHOULD be supported but I'm putting it off for now 
    // until I have an actual use case for it. The (currently apparent)
    // difficulty in getting this to happen also makes it not critical 
    // to implement right away.
    // When we do, the metaenv.lua functions should be designed to use whatever
    // metaprogram is active (I hope its that simple).
    lua.pushstring("exited"_str);
    lua.pushboolean(true);
    lua.settable(I.metaenv__metaenv);
  };

  // NOTE(sushi) I used to do a thing here where the global environment of the 
  //             previous metaenvironment (if any, as in the case of 
  //             processing
  //             an lpp file from within another one) was copied into the one 
  //             were about to execute. This was done so that globals would be 
  //             carried through so that we can emulate C defines. I've decided
  //             to not do this anymore as its not very intuitive in the lua 
  //             sense and comes with some silly problems. If it seems to be 
  //             too much of a bother then maybe I'll return to it another day.
  //             If I do, it should probably be implemented as an option
  //             of lpp.processFile().

  // Finally set the metaenvironment of the metacode and call it.
  TRACE("setting environment of metaprogram\n");
  lua.pushvalue(I.metaenv);
  if (!lua.setfenv(I.metaprogram))
  {
    ERROR("failed to set environment of metaprogram\n");
    return false;
  }

  TRACE("executing metaprogram\n");
  lua.pushvalue(I.metaprogram);
  if (!lua.pcall(0,0,I.errhandler))
  {
    const s32 I_errinfo = lua.gettop();

    if (lua.isfunction())
    {
      lua.pushstring("cancel"_str);
      lua.gettable(I.lpp);

      if (lua.rawequal(I_errinfo, -1))
        return false;
    }

    emitError(I_errinfo, nullptr);

    return false;
  }

  // ~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~ Phase 3
  
  DEBUG("phase 3\n");

  // Process each section generated by Phase 2, joining each Document section 
  // into a single buffer and performing macro expansions.
  
  // Retrieve the document callback invoker.
  TRACE("getting 'lpp.runDocumentSectionCallbacks'\n");
  lua.pushstring("runDocumentSectionCallbacks"_str);
  lua.gettable(I.lpp);
  I.lpp_runDocumentSectionCallbacks = lua.gettop();

  TRACE("getting 'lpp.runFinalCallbacks'\n");
  lua.pushstring("runFinalCallbacks"_str);
  lua.gettable(I.lpp);
  I.lpp_runFinalCallbacks = lua.gettop();

  // Retrieve the __metaenv table.
  TRACE("getting '__metaenv'\n");
  lua.pushstring("__metaenv"_str);
  lua.gettable(I.metaenv);
  I.metaenv_table = lua.gettop();

  // Get the metaenv's macro table.
  TRACE("getting metaenvironment's macro invokers list\n");
  lua.pushstring("macro_invokers"_str);
  lua.gettable(I.metaenv_table);
  I.macro_invokers = lua.gettop();

  // Get the metaenv's macro invocation list.
  TRACE("getting metaenvironment's macro invocation list\n");
  lua.pushstring("macro_invocations"_str);
  lua.gettable(I.metaenv_table);
  I.macro_invocations = lua.gettop();

  lua.pushstring("macro_names"_str);
  lua.gettable(I.metaenv_table);
  I.macro_names = lua.gettop();

  TRACE("processing global scope\n");

  if (!processScopeSections(getCurrentScope()))
    return false;

  output->writeCache(getCurrentScope()->buffer->asStr());

  // Process the final callbacks.
  lua.pushvalue(I.lpp_runFinalCallbacks);
  if (!lua.pcall(0, 0, I.errhandler))
    return false;

  output->cacheLineOffsets();

  for (auto& expansion : expansions)
  {
    printExpansion(input, output, expansion);
  }

  if (lpp->consumers.meta)
  {
    input->cacheLineOffsets();
    output->cacheLineOffsets();
    lpp->consumers.meta->consumeExpansions(*this, expansions);
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Metaprogram::processScopeSections(Scope* scope)
{
  using enum Section::Kind;

  LuaState& lua = lpp->lua;

  // Loop over each section, joining document sections and expanded macros.

  TRACE("processing scope sections\n");

  u64 section_idx = 0;
  for (current_section = scope->sections.head;
       current_section;
       current_section = current_section->next)
  {
    Section* section = current_section->data;
    
    Scope* current_scope = getCurrentScope();

    b8 emit_section = true;
    u64 expansion_start = scope->global_offset + scope->buffer->len;
    u64 expansion_end = 0;

    switch (section->kind)
    {
    case Document:
    case DocumentSpan:
      {
        TRACE("in ", input->name, ": ");
        TRACE("found Document section\n");

        if (!captures.isEmpty() && 
            captures.last()->token_idx == section->token_idx)
        {
          // This document captures an immediate macro or value 
          // result, so adjust the start to whatever the capture
          // started at.
          expansion_start = captures.last()->start;
          captures.pop();
        }

        if (section->kind == DocumentSpan || 
            !section->buffer->asStr().isEmpty())
        {
          lua.pushvalue(I.lpp_runDocumentSectionCallbacks);
          if (!lua.pcall(0, 0, I.errhandler))
            return false;

          // Simple write to the output cache.
          if (section->kind == Document)
          {
            scope->writeBuffer(section->buffer->asStr());
          }
          else
          {
            auto token = parser.lexer.tokens[section->token_idx];
            scope->writeBuffer(input->getStr(token.loc, token.len));
          }
        }
      }  
      break;

    case Macro:
      {
        TRACE("in ", input->name, ": ");
        TRACE("found Macro section\n");
        SectionList::Node* save = current_section;
        defer { current_section = save; };

        // Create a new Scope for the macro.
        // TODO(sushi) this prob doesnt need to be heap.
        Scope* macro_scope = pushScope();
        if (!macro_scope)
          return false;
        defer { popScope(); };

        // Get the macro and execute it.
        TRACE("invoking macro\n");
        lua.pushinteger(section->macro_idx);
        lua.gettable(I.macro_invokers);
        if (!lua.pcall(0, 1))
        {
          const s32 I_errinfo = lua.gettop();

          if (lua.isfunction())
          {
            lua.pushstring("cancel"_str);
            lua.gettable(I.lpp);

            if (lua.rawequal(I_errinfo, -1))
              return false;
          }

          emitError(I_errinfo, macro_scope);
         
          return false;
        }

        // Process the sections produced within the macro, if any.
        if (!processScopeSections(macro_scope))
          return false;

        // Append the result of the scope.
        scope->writeBuffer(macro_scope->buffer->asStr());

        if (!lua.isnil())
          scope->writeBuffer(lua.tostring());

        lua.pop();
      }
      break;

    case MacroImmediate:
      {
        captures.push(
            {section->token_idx, scope->global_offset + scope->buffer->len});
        emit_section = false;
      }
      break;
    }

    expansion_end = 
      scope->global_offset + scope->buffer->len;

    if (emit_section)
    {
      pushExpansion(
          parser.lexer.tokens[section->token_idx].loc,
          expansion_start);

      if (lpp->consumers.meta)
      {
        lpp->consumers.meta->consumeSection(
          *this, 
          *section, 
          expansion_start,
          expansion_end);

      }
    }
  }
  return true;
}

/* ----------------------------------------------------------------------------
 */
String Metaprogram::consumeCurrentScope()
{
  auto* scope = getCurrentScope();
  if (!processScopeSections(scope))
    return nil;

  // TODO(sushi) properly allocate this in a way that it can be cleaned up
  //             do not feel like setting that up right now.
  String out = scope->buffer->asStr().allocateCopy();
  scope->buffer->clear();

  while (scope->sections.head)
  {
    scope->sections.head->data->deinit();
    scope->sections.remove(scope->sections.head);
  }
  return out;
}

/* ----------------------------------------------------------------------------
 */
s32 Metaprogram::mapMetaprogramLineToInputLine(s32 line)
{
  auto line_map = InputLineMap::create();
  defer { line_map.destroy(); };
  generateInputLineMap(&line_map);
    
  for (auto& mapping : line_map)
  {
    if (line <= mapping.metaprogram)
      return mapping.input;
  }
  return -1;
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::generateInputLineMap(InputLineMap* out_map)
{
  for (auto& exp : parser.locmap)
  { 
    Source::Loc from = input->getLoc(exp.from);
    Source::Loc to = meta.getLoc(exp.to);
    out_map->push(
      {
        .metaprogram = s32(to.line),
        .input = s32(from.line)
      });
  }
  // push a eof line
  out_map->push(
      { 
        .metaprogram = out_map->last()->metaprogram+1,
        .input = out_map->last()->input+1
      });
}

/* ----------------------------------------------------------------------------
 */
void Metaprogram::pushExpansion(u64 from, u64 to)
{
  auto* node = expansions.pushTail();
  auto* exp = node->data;
  exp->from = from;
  exp->to = to;
  if (auto* scope = getCurrentScope())
  {
    exp->invoking_macros.init();
    for (;scope; scope = scope->prev)
    {
      if (scope->macro_invocation)
      {
        exp->invoking_macros.push(
            parser.lexer.tokens[scope->macro_invocation->token_idx].loc);
      }
    }
  }
}

}

using namespace lpp;

extern "C"
{
/* IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT !!!!!!
 *
 *
 *
 * IF you CHANGE or ADD STRUCTURES here you MUST make sure that the proper 
 * stuff is UPDATED in cdefs.lua!!!! Giving luajit an incorrect structure def
 * is a REALLY good way to cause runtime errors that are HARD, TOUGH, and 
 * might I even say DIFFICULT to figure out!
 *
 *
 *
 * IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT !!!!!!
 */

// Forward decls, as needed.
// NOTE(sushi) something declared here must not also apply the 
//             LPP_LUAJIT_FFI_FUNC to its definition, since Windows does not 
//             allow a declaration and its definition to have 
//             __declspec(dllexport) applied to it.
LPP_LUAJIT_FFI_FUNC
b8 sectionIsDocument(SectionNode* section);
LPP_LUAJIT_FFI_FUNC
b8 sectionIsMacro(SectionNode* section);

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramAddMacroSection(
    Metaprogram* mp, 
    u64 start, 
    u64 macro_idx)
{
  mp->addMacroSection(start, macro_idx);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramAddMacroImmediateSection(
    Metaprogram* mp,
    u64 start)
{
  mp->addMacroImmediateSection(start);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramAddDocumentSection(Metaprogram* mp, u64 start, String raw)
{
  mp->addDocumentSection(start, raw);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramAddDocumentSpanSection(Metaprogram* mp, u64 start)
{
  mp->addDocumentSpanSection(start);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
String metaprogramGetOutputSoFar(Metaprogram* mp)
{
  return mp->output->cache.asStr();
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
void metaprogramTrackExpansion(Metaprogram* mp, u64 from, u64 to)
{
  mp->expansions.pushTail({from, to});
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
s32 metaprogramMapMetaprogramLineToInputLine(Metaprogram* mp, s32 line)
{
  return mp->mapMetaprogramLineToInputLine(line);
}

/* ----------------------------------------------------------------------------
 */
struct LineAndColumn
{
  u64 line;
  u64 column;
};

LPP_LUAJIT_FFI_FUNC
LineAndColumn metaprogramMapInputOffsetToLineAndColumn(
    Metaprogram* mp, 
    u64 offset)
{
  assert(mp);
  assert(offset < mp->input->cache.len);
  LineAndColumn lac;
  Source::Loc loc = mp->input->getLoc(offset);
  lac.line = loc.line;
  lac.column = loc.column;
  return lac;
}

/* ----------------------------------------------------------------------------
 *  Processes the sections of the current scope and returns the result as a 
 *  String. This does not end the scope.
 *
 *  TODO(sushi) a version of this could be made that forks the scope and 
 *              returns it to be evaluated later.
 */
LPP_LUAJIT_FFI_FUNC
String metaprogramConsumeCurrentScopeString(Metaprogram* mp)
{
  return mp->consumeCurrentScope();
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* metaprogramGetTopMacroInvocation(Metaprogram* mp)
{
  assert(sectionIsMacro(mp->current_section));

  SectionNode* walk = mp->current_section;

  while (sectionIsMacro(walk) && walk->prev)
    walk = walk->prev;

  return walk;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
Metaprogram* metaprogramGetPrev(Metaprogram* mp)
{
  assert(mp);
  return mp->prev; 
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
Scope* metaprogramGetCurrentScope(Metaprogram* mp)
{
  assert(mp);
  return mp->getCurrentScope();
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
Scope* scopeGetPrev(Scope* scope)
{
  assert(scope);
  return scope->prev;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
u64 scopeGetInvokingMacroIdx(Scope* scope)
{
  assert(scope);
  return (scope->macro_invocation? scope->macro_invocation->macro_idx : 0);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* metaprogramGetNextSection(Metaprogram* mp)
{
  return mp->current_section->next;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* metaprogramGetCurrentSection(Metaprogram* mp)
{
  return mp->current_section;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
String sectionGetString(SectionNode* section)
{
  return section->data->buffer->asStr();
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* sectionNext(SectionNode* section)
{
  return section->next;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
SectionNode* sectionPrev(SectionNode* section)
{
  return section->prev;
}

/* ----------------------------------------------------------------------------
 */
b8 sectionIsMacro(SectionNode* section)
{
  return section->data->kind == Section::Kind::Macro;
}

/* ----------------------------------------------------------------------------
 */
b8 sectionIsDocument(SectionNode* section)
{
  return section->data->kind == Section::Kind::Document;
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
b8 sectionInsertString(SectionNode* section, u64 offset, String s)
{
  assert(sectionIsDocument(section));
  return section->data->insertString(offset, s);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
b8 sectionAppendString(SectionNode* section, String s)
{
  assert(sectionIsDocument(section));
  return section->data->insertString(section->data->buffer->len, s);
}

/* ----------------------------------------------------------------------------
 */
LPP_LUAJIT_FFI_FUNC
b8 sectionConsumeFromBeginning(SectionNode* section, u64 len)
{
  assert(sectionIsDocument(section));
  return section->data->consumeFromBeginning(len);
}

/* ----------------------------------------------------------------------------
 *  Get the offset into the input file where this section starts.
 */
LPP_LUAJIT_FFI_FUNC
u64 sectionGetStartOffset(SectionNode* section)
{
  // assert(section);
  // return section->data->start_offset;
  assert(!"reimplement if ever used");
  return 0;
}

}

