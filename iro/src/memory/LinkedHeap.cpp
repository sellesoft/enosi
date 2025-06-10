#include "LinkedHeap.h"
#include "../Logger.h"

namespace iro::mem
{

static Logger log = 
  Logger::create("linkheap"_str, Logger::Verbosity::Debug);

/* ============================================================================
 */
struct Node
{
  Node* prev = this;
  Node* next = this;
};

/* --------------------------------------------------------------------------
 */
static void listInit(Node* n)
{
  n->prev = n->next = n;
}

/* --------------------------------------------------------------------------
 */
static void listInsertAfter(Node* a, Node* b)
{
  Node* an = a->next;
  Node* bp = b->prev;

  a->next = b;
  b->prev = a;
  bp->next = an;
  an->prev = bp;
}

/* --------------------------------------------------------------------------
 */
static void listInsertBefore(Node* a, Node* b)
{
  Node* ap = a->prev;
  Node* bp = b->prev;

  ap->next = b;
  b->prev = ap;
  b->next = a;
  a->prev = bp;
}

/* --------------------------------------------------------------------------
 */
static void listRemove(Node* n)
{
  n->prev->next = n->next;
  n->next->prev = n->prev;
  n->next = n->prev = n;
}

/* --------------------------------------------------------------------------
 */
static void listPush(Node** list, Node* n)
{
  if (*list != nullptr)
    listInsertBefore(*list, n);
  *list = n;
}

/* --------------------------------------------------------------------------
 */
static Node* listPop(Node** list)
{
  Node* d1 = *list;
  Node* d2 = d1->next;
  listRemove(d1);
  if (d1 == d2)
    *list = nullptr;
  else
    *list = d2;
  return d1;
}

/* --------------------------------------------------------------------------
 */
static void listRemoveFrom(Node** list, Node* d2)
{
  if (*list == d2)
    listPop(list);
  else
    listRemove(d2);
}

/* ============================================================================
 */
struct Chunk
{
  Node node;
  b8 used;

  union 
  {
    u8* data;
    Node free;
  };
};
static constexpr u64 s_ChunkHeaderSize = offsetof(Chunk, data);
static constexpr u64 s_MinAllocatedSize = sizeof(Node);

/* --------------------------------------------------------------------------
 */
static void chunkInit(Chunk* chunk)
{
  listInit(&chunk->node);
  listInit(&chunk->free);
  chunk->used = false;
}

/* --------------------------------------------------------------------------
 */
static u64 chunkCalcSize(Chunk* chunk)
{
  return (u8*)chunk->node.next - (u8*)&chunk->node - s_ChunkHeaderSize;
}

/* --------------------------------------------------------------------------
 */
static s32 chunkCalcSlotIndex(u64 size)
{
  s32 n = -1;
  while (size > 0)
  {
    n += 1;
    size /= 2;
  }
  return n;
}

/* --------------------------------------------------------------------------
 */
static Chunk* chunkGetNext(Chunk* chunk)
{
  return (Chunk*)((u8*)chunk->node.next - offsetof(Chunk, node));
}

/* --------------------------------------------------------------------------
 */
static Chunk* chunkGetPrev(Chunk* chunk)
{
  return (Chunk*)((u8*)chunk->node.prev - offsetof(Chunk, node));
}

/* --------------------------------------------------------------------------
 */
static Chunk* chunkFromFreeNode(Node* n)
{
  return ((Chunk*)((u8*)n - offsetof(Chunk, free)));
}

/* --------------------------------------------------------------------------
 */
static void pushFreeChunk(Chunk** list, Chunk* chunk)
{
  Node* head = &(*list)->free;
  if (*list == nullptr)
    head = nullptr;
  listPush(&head, &chunk->free);
  *list = chunkFromFreeNode(head);
}

/* --------------------------------------------------------------------------
 */
static Chunk* popFreeChunk(Chunk** list)
{
  Node* head = &(*list)->free;
  Node* result = listPop(&head);
  if (head == nullptr)
    *list = nullptr;
  else 
    *list = chunkFromFreeNode(head);
  return chunkFromFreeNode(result);
}

/* --------------------------------------------------------------------------
 */
static void removeFreeChunk(Chunk** list, Chunk* chunk)
{
  Node* head = &(*list)->free;
  listRemoveFrom(&head, &chunk->free);
  if (head == nullptr)
    *list = nullptr;
  else
    *list = chunkFromFreeNode(head);
}

/* --------------------------------------------------------------------------
 */
b8 LinkedHeap::init(void* ptr, u64 size)
{
  u8* mem_start = (u8*)(((u64)ptr + c_Alignment - 1) & (~(c_Alignment - 1)));
  u8* mem_end = (u8*)(((u64)ptr + size) & (~(c_Alignment - 1)));

  // Assign addresses of boundry chunks and the initial center.
  first = (Chunk*)mem_start;
  last = (Chunk*)mem_end - 1;
  Chunk* second = first + 1;

  // Init.
  chunkInit(first);
  chunkInit(second);
  chunkInit(last);

  // Link initial nodes.
  listInsertAfter(&first->node, &second->node);
  listInsertAfter(&second->node, &last->node);

  // Mark first/last as used so they never get merged.
  first->used = true;
  last->used = true;

  // Push the free chunk into some slot.
  u64 len = chunkCalcSize(second);
  s32 slotidx = chunkCalcSlotIndex(len);
  pushFreeChunk(&free_chunks[slotidx], second);

  free_bytes = len - s_ChunkHeaderSize;
  meta_bytes = sizeof(Chunk) * 2 + s_ChunkHeaderSize;

  return true;
}

/* --------------------------------------------------------------------------
 */
void LinkedHeap::deinit()
{
  // TODO(sushi) validate that everything was deallocated here.
}

/* --------------------------------------------------------------------------
 */
void* LinkedHeap::allocate(u64 size)
{
  size = (size + c_Alignment - 1) & (~(c_Alignment - 1));
  size = max<u64>(size, s_MinAllocatedSize);

  s32 slot_idx = chunkCalcSlotIndex(size - 1) + 1;

  if (slot_idx >= c_NumSizes)
    return nullptr;

  while (free_chunks[slot_idx] == nullptr)
  {
    slot_idx += 1;
    if (slot_idx >= c_NumSizes)
      return nullptr;
  }

  Chunk* chunk = popFreeChunk(&free_chunks[slot_idx]);
  u64 chunk_size = chunkCalcSize(chunk);

  u64 new_chunk_len = 0;
  if (size + sizeof(Chunk) <= chunk_size)
  {
    // Split the chunk.
    Chunk* new_chunk = (Chunk*)(((u8*)chunk) + s_ChunkHeaderSize + size);
    chunkInit(new_chunk);
    listInsertAfter(&chunk->node, &new_chunk->node);
    new_chunk_len = chunkCalcSize(new_chunk);
    s32 new_chunk_slot = chunkCalcSlotIndex(new_chunk_len);
    pushFreeChunk(&free_chunks[new_chunk_slot], new_chunk);

    meta_bytes += s_ChunkHeaderSize;
    free_bytes += new_chunk_len - s_ChunkHeaderSize;
  }

  chunk->used = true;
  free_bytes -= chunk_size;
  used_bytes += chunk_size - new_chunk_len - s_ChunkHeaderSize;

  return chunk->data;
}

/* --------------------------------------------------------------------------
 */
void LinkedHeap::free(void* ptr)
{
  Chunk* chunk = (Chunk*)((u8*)ptr - s_ChunkHeaderSize);
  Chunk* next = chunkGetNext(chunk);
  Chunk* prev = chunkGetPrev(chunk);

  used_bytes -= chunkCalcSize(chunk);

  if (!next->used)
  {
    u64 size = chunkCalcSize(next);
    s32 slot_idx = chunkCalcSlotIndex(size);
    removeFreeChunk(&free_chunks[slot_idx], next);
    free_bytes -= size - s_ChunkHeaderSize;
    listRemove(&next->node);

    meta_bytes -= s_ChunkHeaderSize;
    free_bytes += s_ChunkHeaderSize;
  }

  if (!prev->used)
  {
    // Remove the prev node.
    {
      u64 size = chunkCalcSize(prev);
      s32 slot_idx = chunkCalcSlotIndex(size);
      removeFreeChunk(&free_chunks[slot_idx], prev);
      free_bytes -= size - s_ChunkHeaderSize;
      listRemove(&prev->node);
    }

    {
      u64 size = chunkCalcSize(prev);
      s32 slot_idx = chunkCalcSlotIndex(size);
      pushFreeChunk(&free_chunks[slot_idx], prev);
      free_bytes += size - s_ChunkHeaderSize;
    }

    meta_bytes -= s_ChunkHeaderSize;
    free_bytes += s_ChunkHeaderSize;
  }
  else
  {
    chunk->used = false;
    listInit(&chunk->node);
    
    {
      u64 size = chunkCalcSize(prev);
      s32 slot_idx = chunkCalcSlotIndex(size);
      pushFreeChunk(&free_chunks[slot_idx], chunk);
    }
  }
}

}
