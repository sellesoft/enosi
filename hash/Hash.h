#ifndef _hash_Hash_h
#define _hash_Hash_h

#include "iro/Common.h"
#include "iro/Unicode.h"

/* ----------------------------------------------------------------------------
 */
#if HASH_CACHE_ENABLED
b8 initHashCache(const char* cache_path_str);
void deinitHashCache();
#endif // #if HASH_CACHE_ENABLED

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC u32 enosiHash32(void* data, u64 len, u32 seed);
EXPORT_DYNAMIC u64 enosiHash64(void* data, u64 len, u64 seed);

namespace hash
{

/* ============================================================================
 */
struct Hash32
{
  u32 value;

  inline bool operator ==(const Hash32& rhs) const
  {
    return this->value == rhs.value;
  }
};

/* ----------------------------------------------------------------------------
 */
inline Hash32 hash32(iro::String x)
{
  return {enosiHash32(x.ptr, x.len, 0)};
}

/* ----------------------------------------------------------------------------
 */
inline Hash32 hash32(void* data, u64 len)
{
  return {enosiHash32(data, len, 0)};
}

/* ----------------------------------------------------------------------------
 */
iro::StackString<64> lookup(Hash32 hash);

/* ============================================================================
 */
struct Hash64
{
  u64 value;

  inline bool operator ==(const Hash64& rhs) const
  {
    return this->value == rhs.value;
  }
};

/* ----------------------------------------------------------------------------
 */
inline Hash64 hash64(iro::String x)
{
  return {enosiHash64(x.ptr, x.len, 0)};
}

/* ----------------------------------------------------------------------------
 */
inline Hash64 hash64(void* data, u64 len)
{
  return {enosiHash64(data, len, 0)};
}

/* ----------------------------------------------------------------------------
 */
iro::StackString<64> lookup(Hash64 hash);

} // namespace hash

#endif // _hash_Hash_h