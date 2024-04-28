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

}

#endif // _iro_memory_h

