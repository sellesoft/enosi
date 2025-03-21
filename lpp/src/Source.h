/*
 *  Central representation of some source given to lpp for preprocessing.
 *  
 *  Handles translating source locations (byte offsets) to other information
 *  such as raw strings and line+columns as well as caching useful information.
 *
 *  TODO(sushi) this should probably be expanded to manage all of the 
 *              information related to some 'source', eg. its lexer, parser,
 *              tokens, metaprogram, each intermediate buffer we need, etc.
 */

#ifndef _lpp_Source_h
#define _lpp_Source_h

#include "iro/Common.h"
#include "iro/Unicode.h"
#include "iro/containers/Array.h"
#include "iro/io/IO.h"

using namespace iro;

/* ============================================================================
 */
struct Source 
{
  String name;

  // A cache of this source's content.
  // TODO(sushi) figure out when we're given memory that we can just 
  //             keep a view over.
  //             or maybe later stuff should be designed to get 
  //             content on demand so we dont store so much memory?
  io::Memory cache;

  // Text created by the lexer.
  io::Memory virtual_cache;

  // Byte offsets into this source where we find newlines.
  b8 line_offsets_calculated;
  Array<u64> line_offsets;

  /* ==========================================================================
   */
  struct Loc
  { 
    u64 line;
    u64 column;
  };


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */


  b8   init(String name);
  void deinit();

  // Writes to the cache and handles some state such as if 
  // line offsets are calculated.
  b8 writeCache(Bytes slice);

  // Caches line offsets in the cached buffer.
  b8 cacheLineOffsets();

  String getStr(u64 offset, u64 length);
  String getVirtualStr(u64 offset, u64 length);

  // Translate a byte offset into this source to 
  // a line and column.
  Loc getLoc(u64 offset) const;
};

#endif // _lpp_source_h
