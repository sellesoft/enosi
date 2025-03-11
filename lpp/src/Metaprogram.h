/*
 *  The lua program generated by the Parser that outputs the final result.
 */

#ifndef _lpp_Metaprogram_h
#define _lpp_Metaprogram_h

#include "iro/Common.h"
#include "iro/containers/Array.h"
#include "iro/containers/Pool.h"
#include "iro/containers/LinkedPool.h"
#include "iro/containers/List.h"
#include "iro/fs/Path.h"

#include "Source.h"
#include "Parser.h"

namespace lpp
{

struct MetaprogramContext;
struct Lpp;

using namespace iro;

struct Section;
struct Cursor;
struct Expansion;
struct Metaprogram;

typedef SLinkedPool<io::Memory> BufferPool;

/* ============================================================================
 */
typedef DLinkedPool<Section> SectionPool;
typedef SectionPool::List SectionList;
typedef SectionPool::Node SectionNode;

struct Section
{
  enum class Kind : u8
  {
    Invalid,
    Document,
    DocumentSpan,
    Macro,
    MacroImmediate,
  };

  Kind kind = Kind::Invalid;

  // The Token at which this section starts. More detail about what this 
  // section came from is attainable from the kind of Token that started it.
  u64 token_idx = -1;

  io::Memory* buffer = nullptr;

  SectionNode* node = nullptr;

  u64 macro_idx = -1;

  b8 initDocument(
      u64 token_idx,
      String raw, 
      SectionNode* node,
      io::Memory* buffer);

  b8 initDocumentSpan(
    u64 token_idx,
    SectionNode* node);

  b8 initMacro(
      u64 token_idx,
      u64 macro_idx, 
      SectionNode* node);

  b8 initMacroImmediate(
      u64 token_idx,
      SectionNode* node);

  void deinit();

  b8 insertString(u64 start, String s);
  b8 consumeFromBeginning(u64 len);
};

}

namespace iro::io
{

static s64 format(io::IO* io, lpp::Section::Kind& kind)
{
  switch (kind)
  {
#define c(x) case lpp::Section::Kind::x: return format(io, STRINGIZE(x));
  c(Invalid);
  c(Document);
  c(Macro);
#undef c
    default: return format(io, "INVALID SECTION KIND");
  }
}

}

namespace lpp
{

/* ============================================================================
 */
typedef Pool<Cursor> CursorPool;

struct Cursor
{
  SectionNode* creator = nullptr;
  SectionNode* section = nullptr;

  // the codepoint at the beginning of 'range'
  utf8::Codepoint current_codepoint;
  String range;

  u64 offset; // into current section
};

/* ============================================================================
 */
typedef DLinkedPool<Expansion> ExpansionList;

struct Expansion
{
  u64 from;
  u64 to;
  Array<u64> invoking_macros;
};

/* ============================================================================
 *  A list of sections and the buffer they are joined into.
 *  We also store information about how the scope started. There is always the 
 *  global level scope then each macro invocation starts its own scope. 
 */
struct Scope
{
  Scope* prev;
  SectionList sections;
  io::Memory* buffer;

  // The macro section where this scope was created.
  // Null when this is the global scope.
  Section* macro_invocation;

  u64 global_offset = 0;

  // NOTE(sushi) its assumed the passed buffer is already open.
  b8   init(Scope* prev, io::Memory* buffer, Section* macro_invocation);
  void deinit();

  void writeBuffer(Bytes bytes) { buffer->write(bytes); }
};
typedef SLinkedPool<Scope> ScopePool;

/* ============================================================================
 */
struct MetaprogramDiagnostic
{
  Source* source;
  u64     loc;
  String  message;
};

/* ============================================================================
 */
struct MetaprogramConsumer
{
  virtual void consumeDiag(
    const Metaprogram& mp, 
    const MetaprogramDiagnostic& diag) {}

  virtual void consumeSection(
    const Metaprogram& mp,
    const Section& section,
    u64 expansion_start,
    u64 expansion_end) {}

  virtual void consumeExpansions(
    const Metaprogram& mp,
    const ExpansionList& exps) {}
};

/* ============================================================================
 */
struct Metaprogram
{
  Lpp* lpp;

  Metaprogram* prev;

  BufferPool buffers;

  SectionPool   sections;
  ExpansionList expansions;

  ScopePool scope_stack;

  struct Capture
  {
    u64 token_idx;
    u64 start;
  };

  Array<Capture> captures;

  io::IO* instream;
  Source* input;
  Source* output;
  Source meta;

  SectionNode* current_section;

  Parser parser;
  io::Memory parsed_program;

  // A mapping from a line in the generated metaprogram 
  // to the corresponding line in the original input. 
  // This lets us display the proper location of an error
  // in the input file when something goes wrong in lua.
  // 
  // This is only generated when needed through generateInputLineMap.
  struct InputLineMapping { s32 metaprogram; s32 input; };
  typedef Array<InputLineMapping> InputLineMap;

  b8 init(
    Lpp*         lpp, 
    io::IO*      instream, 
    Source*      input, 
    Source*      output,
    Metaprogram* prev);

  void deinit();

  b8 run();

  Scope* pushScope();
  void   popScope();
  Scope* getCurrentScope();

  void addDocumentSection(u64 start, String s);
  void addDocumentSpanSection(u64 token_idx);
  void addMacroSection(s64 start, u64 macro_idx);
  void addMacroImmediateSection(u64 start);

  struct StackEnt
  {
    String src;
    s32 line;
    String name;
    Metaprogram* mp;
  };

  StackEnt getStackEnt(s32 stack_idx, s32 ent_idx);

  void emitError(s32 stack_idx, Scope* scope);

  b8 processScopeSections(Scope* scope);

  String consumeCurrentScope();

  s32 mapMetaprogramLineToInputLine(s32 line);

  // Lazily generates the input line map and returns a pointer to it.
  void generateInputLineMap(InputLineMap* out_map);

  void pushExpansion(u64 from, u64 to);

  // Indexes on the lua stack where important stuff is.
  // Eventually if lpp is ever 'stable' this should be 
  // removed in favor of using const vars set to where we know 
  // these things are on the stack, jsut doing this atm as it 
  // makes working with lua's stack easier in code that changes 
  // often.
  struct 
  {
    s32 errhandler;
    s32 metaprogram;
    s32 metaenv;
    s32 metaenv__metaenv; // :P
    s32 lpp;
    s32 prev_context;
    s32 prev_metaenv;
    s32 metaenv_table;
    s32 macro_invokers;
    s32 macro_invocations;
    s32 macro_names;
    s32 lpp_runDocumentSectionCallbacks;
    s32 lpp_runFinalCallbacks;
  } I;
};

}

extern "C"
{
  // TODO(sushi) maybe someday put the lua api here.
  //             Its really not necessary and is just another place I have 
  //             to update declarations at.
}

#endif // _lpp_metaprogram_h
