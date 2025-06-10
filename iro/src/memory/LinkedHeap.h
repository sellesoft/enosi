/*
 *  A linked list heap structure. 
 *
 *  This structure was intially based on the code provided by 
 *  Goswin von Brederlow (goswin-v-b@web.de), found on the OSDev wiki at
 *  wiki.osdev.org/User:Mrvn/LinkedListBucketHeapImplementation.
 */

#ifndef _iro_LinkedHeap_h
#define _iro_LinkedHeap_h

#include "../Common.h"

namespace iro::mem
{

struct Chunk;

/* ============================================================================
 */
struct LinkedHeap
{
  enum 
  {
    c_Alignment = 4,
    c_NumSizes = 32,
  };

  Chunk* free_chunks[c_NumSizes] = {};

  u64 free_bytes = 0;
  u64 used_bytes = 0;
  u64 meta_bytes = 0;

  Chunk* first = nullptr;
  Chunk* last  = nullptr;

  b8 init(void* ptr, u64 size);
  void deinit();

  void* allocate(u64 size);
  void  free(void* ptr);
};

}

#endif
