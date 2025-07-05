/*
 *  Basic memory functions
 */

#ifndef _iro_Memory_h
#define _iro_Memory_h

#include "../Common.h"

namespace iro::mem
{

void copy(void* dst, void* src, u64 bytes);
void move(void* dst, void* src, u64 bytes);
void zero(void* ptr, u64 bytes);
b8 equal(const void* lhs, const void* rhs, u64 bytes);

template<typename T>
void zeroStruct(T* ptr) { zero(ptr, sizeof(T)); }

template<typename T>
b8 equal(const T& lhs, const T& rhs) { return equal(&lhs, &rhs, sizeof(T)); }

}

#endif // _iro_memory_h

