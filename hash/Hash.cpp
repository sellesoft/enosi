#include "Hash.h"
#include "iro/Common.h"
#include "iro/Unicode.h"

#if HASH_CACHE_ENABLED
#include "iro/containers/Array.h"
#include "iro/memory/Memory.h"
#include "iro/fs/File.h"
#include <string.h>
#endif // #if HASH_CACHE_ENABLED

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include <xxhash.h>
#include <stdio.h>

#if HASH_CACHE_ENABLED

/* ============================================================================
 */
struct HashCacheEntryU32
{
  u32 hash;
  u32 string_offset;
  u32 string_length;
};

/* ============================================================================
 */
struct HashCacheEntryU64
{
  u64 hash;
  u32 string_offset;
  u32 string_length;
};

/* ============================================================================
 */
constexpr u64 HASH_CACHE_BUFFER_MAX_SIZE = unit::megabytes(1);
static u8 s_hash_cache_string_buffer[HASH_CACHE_BUFFER_MAX_SIZE];
static u32 s_hash_cache_string_buffer_length = 0;
static Array<HashCacheEntry32> s_hash_cache32;
static Array<HashCacheEntry64> s_hash_cache64;
static iro::String s_cache_path;

/* ----------------------------------------------------------------------------
 */
static void saveSource(Hash32 hash, void* data, u64 len)
{
  if (s_cache_path.isEmpty())
    return;

  len = min(64, len);

  if (s_hash_cache32.len() == 0)
  {
    s_hash_cache32.add({hash, data, len});
  }
  else
  {
    s32 left = 0;
    s32 right = s_hash_cache32.len() - 1;
    s32 middle = 0;
    while (left <= right)
    {
      middle = left + ((right - left) / 2);
      if (s_hash_cache32[middle].hash == hash)
      {
        assert(0 == memcmp(&s_hash_cache_string_buffer[
          s_hash_cache32[middle].string_offset], data, len));
        return;
      }
      else if (s_hash_cache32[middle].hash < hash)
      {
        left = middle + 1;
        middle = left + ((right - left) / 2);
      }
      else
      {
        right = middle - 1;
      }
    }

    s_hash_cache32.insert(middle, {hash, data, len});
  }
}

/* ----------------------------------------------------------------------------
 */
static void saveSource(Hash64 hash, void* data, u64 len)
{
  if (s_cache_path.isEmpty())
    return;

  len = min(64, len);

  if (s_hash_cache64.len() == 0)
  {
    s_hash_cache64.add({hash, data, len});
  }
  else
  {
    s32 left = 0;
    s32 right = s_hash_cache64.len() - 1;
    s32 middle = 0;
    while (left <= right)
    {
      middle = left + ((right - left) / 2);
      if (s_hash_cache64[middle].hash == hash)
      {
        assert(0 == memcmp(&s_hash_cache_string_buffer[
          s_hash_cache64[middle].string_offset], data, len));
        return;
      }
      else if (s_hash_cache64[middle].hash < hash)
      {
        left = middle + 1;
        middle = left + ((right - left) / 2);
      }
      else
      {
        right = middle - 1;
      }
    }

    s_hash_cache64.insert(middle, {hash, data, len});
  }
}

/* ----------------------------------------------------------------------------
 */
static HashCacheEntry32* findSource(Hash32 hash)
{
  if (s_cache_path.isEmpty())
    return nullptr;

  s32 left = 0;
  s32 right = s_hash_cache32.len() - 1;
  while (left <= right)
  {
    s32 middle = left + ((right - left) / 2);
    if (s_hash_cache32[middle].hash <= hash)
    {
      left = middle + 1;
    }
    else if (s_hash_cache32[middle].hash > hash)
    {
      right = middle - 1;
    }
    else
    {
      return &s_hash_cache32[middle];
    }
  }

  return nullptr;
}

/* ----------------------------------------------------------------------------
 */
static HashCacheEntry64* findSource(Hash64 hash)
{
  if (s_cache_path.isEmpty())
    return nullptr;

  s32 left = 0;
  s32 right = s_hash_cache64.len() - 1;
  while (left <= right)
  {
    s32 middle = left + ((right - left) / 2);
    if (s_hash_cache64[middle].hash <= hash)
    {
      left = middle + 1;
    }
    else if (s_hash_cache64[middle].hash > hash)
    {
      right = middle - 1;
    }
    else
    {
      return &s_hash_cache64[middle];
    }
  }

  return nullptr;
}

/* ----------------------------------------------------------------------------
 */
b8 initHashCache(const char* cache_path_str)
{
  if (!s_cache_path.isEmpty())
    return true;

  auto cache_path = iro::String::from(cache_path_str, strlen(cache_path_str));
  auto cache_file = iro::fs::File::from(cache_path, iro::fs::OpenFlag::Read);
  if (!cache_file.isOpen())
    return false;
  defer { cache_file.close(); };

  iro::fs::FileInfo file_info = cache_file.getInfo();
  if (file_info.size == 0)
  {
    s_hash_cache32.init(512);
    s_hash_cache64.init(512);
  }
  else
  {
    u32 hash_cache32_entries;
    Bytes buffer = {&hash_cache32_entries, sizeof(hash_cache32_entries)};
    if (cache_file.read(buffer) != sizeof(hash_cache32_entries))
      return false;

    u32 hash_cache64_entries;
    buffer = {&hash_cache64_entries, sizeof(hash_cache64_entries)};
    if (cache_file.read(buffer) != sizeof(hash_cache64_entries))
      return false;

    u32 string_buffer_size;
    buffer = {&string_buffer_size, sizeof(string_buffer_size)};
    if (cache_file.read(buffer) != sizeof(string_buffer_size))
      return false;
    if (string_buffer_size > HASH_CACHE_BUFFER_MAX_SIZE)
      return false;

    s_hash_cache32.init(2 * alignUp(hash_cache32_entries, 256));
    s_hash_cache32.len() = hash_cache32_entries;

    s_hash_cache64.init(2 * alignUp(hash_cache64_entries, 256));
    s_hash_cache64.len() = hash_cache64_entries;

    buffer = {s_hash_cache32.ptr,
              s_hash_cache32_entries * sizeof(HashCacheEntry32)};
    if (cache_file.read(buffer) != buffer.len)
      return false;

    buffer = {s_hash_cache64.ptr,
              s_hash_cache64_entries * sizeof(HashCacheEntry64)};
    if (cache_file.read(buffer) != buffer.len)
      return false;

    buffer = {s_hash_cache_string_buffer, s_string_buffer_size};
    if (cache_file.read(buffer) != buffer.len)
      return false;
  }

  s_cache_path = cache_path.allocateCopy();
  return true;
}

/* ----------------------------------------------------------------------------
 */
void deinitHashCache()
{
  if (s_cache_path.isEmpty())
    return;

  auto open_flags = iro::fs::OpenFlag::Write | iro::fs::OpenFlag::Create;
  auto cache_file = iro::fs::File::from(s_cache_path, open_flags);
  if (!cache_file.isOpen())
    return;
  defer { cache_file.close(); };

  u32 hash_cache32_entries = s_hash_cache32.len();
  Bytes buffer = {&hash_cache32_entries, sizeof(hash_cache32_entries)};
  if (cache_file.write(buffer) != buffer.len)
    return;

  u32 hash_cache64_entries = s_hash_cache64.len();
  buffer = {&hash_cache64_entries, sizeof(hash_cache64_entries)};
  if (cache_file.write(buffer) != buffer.len)
    return;

  u32 string_buffer_size = s_hash_cache_string_buffer_length;
  buffer = {&string_buffer_size, sizeof(string_buffer_size)};
  if (cache_file.write(buffer) != buffer.len)
    return;

  buffer = {s_hash_cache32.ptr,
            hash_cache32_entries * sizeof(HashCacheEntry32)};
  if (cache_file.write(buffer) != buffer.len)
    return;

  buffer = {s_hash_cache64.ptr,
            hash_cache64_entries * sizeof(HashCacheEntry64)};
  if (cache_file.write(buffer) != buffer.len)
    return;

  buffer = {s_hash_cache_string_buffer, s_string_buffer_size};
  if (cache_file.write(buffer) != buffer.len)
    return;

  s_hash_cache32.clear();
  s_hash_cache64.clear();
  s_hash_cache_string_buffer_length = 0;
  s_cache_path = {};
}

#endif // #if HASH_CACHE_ENABLED

/* ----------------------------------------------------------------------------
 */
u32 enosiHash32(void* data, u64 len, u32 seed)
{
  u32 hash = XXH32(data, len, seed);
#if HASH_CACHE_ENABLED
  saveSource(hash, data, len);
#endif // #if HASH_CACHE_ENABLED
  return hash;
}

/* ----------------------------------------------------------------------------
 */
u64 enosiHash64(void* data, u64 len, u64 seed)
{
  u64 hash = XXH3_64bits_withSeed(data, len, seed);
#if HASH_CACHE_ENABLED
  saveSource(hash, data, len);
#endif // #if HASH_CACHE_ENABLED
  return hash;
}

namespace hash
{

/* ----------------------------------------------------------------------------
 */
iro::StackString<64> lookup(Hash32 hash)
{
  iro::StackString<64> result;

#if HASH_CACHE_ENABLED
  HashCacheEntry32* entry = findSource(hash);
  if (entry)
  {
    assertrange(entry->string_length, 0, 64);
    mem::copy(result.ptr,
              s_hash_cache_string_buffer + entry->string_offset,
              min(64, entry->string_length));
    return result;
  }
#endif // #if HASH_CACHE_ENABLED

  int length = snprintf((char*)result.ptr, 64, "32#%X", hash.value);
  result.len = (u64)min(64, length);
  assertrange(length, 0, 64);

  return result;
}

/* ----------------------------------------------------------------------------
 */
iro::StackString<64> lookup(Hash64 hash)
{
  iro::StackString<64> result;

#if HASH_CACHE_ENABLED
  HashCacheEntry64* entry = findSource(hash);
  if (entry)
  {
    assertrange(entry->string_length, 0, 64);
    mem::copy(result.ptr,
              s_hash_cache_string_buffer + entry->string_offset,
              min(64, entry->string_length));
    return result;
  }
#endif // #if HASH_CACHE_ENABLED

  int length = snprintf((char*)result.ptr, 64, "64#%llX", hash.value);
  result.len = (u64)min(64, length);
  assertrange(length, 0, 64);

  return result;
}

} // namespace hash