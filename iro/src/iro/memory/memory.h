/*
 *  Basic memory functions
 */

#ifndef _iro_memory_h
#define _iro_memory_h

#include "../common.h"

namespace iro::mem
{

void copy(void* dst, void* src, u64 bytes);
void move(void* dst, void* src, u64 bytes);
void zero(void* ptr, u64 bytes);

template<typename T>
void zeroStruct(T* ptr) { zero(ptr, sizeof(T)); }

}

#endif // _iro_memory_h

