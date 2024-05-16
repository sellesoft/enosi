#include "io.h"

#include "stdlib.h"
#include "stdio.h"
#include "../memory/memory.h"

// TODO(sushi) platform layers
#include "fcntl.h"
#include "sys/stat.h"
#include "unistd.h"

#include "sys/stat.h"

#include "assert.h"

namespace iro::io
{

/* ------------------------------------------------------------------------------------------------ Memory::growIfNeeded
 */
void Memory::growIfNeeded(s64 wanted_space)
{
	if (space - len >= wanted_space)
		return;

	while (space - len < wanted_space)
		space *= 2;

	buffer = (u8*)allocator->reallocate(buffer, space);
}

/* ------------------------------------------------------------------------------------------------ Memory::open
 */
b8 Memory::open(s32 initial_space, mem::Allocator* allocator)
{
	this->allocator = allocator;

	if (flags.test(Flag::Open))
		return true;

	flags.set(Flag::Writable);
	flags.set(Flag::Readable);

	space = initial_space + 1;
	pos = len = 0;
	buffer = (u8*)allocator->allocate(space);
	buffer[initial_space] = 0;

	if (buffer != nullptr)
	{
		flags.set(Flag::Open);
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------------------------------------ Memory::close
 */
void Memory::close()
{
	if (!isOpen())
		return;

	allocator->free(buffer);
	buffer = nullptr;
	len = space = 0;
	pos = 0;

	flags.unset(Flag::Open);

	*this = nil;
}

/* ------------------------------------------------------------------------------------------------ Memory::reserve
 */
Bytes Memory::reserve(s32 wanted_space)
{
	growIfNeeded(wanted_space+1);
	
	return {buffer + len, u64(wanted_space)};
}

/* ------------------------------------------------------------------------------------------------ Memory::commit
 */
void Memory::commit(s32 committed_space)
{
	assert(len + committed_space <= space);
	len += committed_space;
	buffer[len] = 0;
}

/* ------------------------------------------------------------------------------------------------ Memory::write
 */
s64 Memory::write(Bytes slice)
{
	growIfNeeded(slice.len+1);

	mem::copy(buffer + len, slice.ptr, slice.len);
	len += slice.len;

	buffer[len] = 0;

	return slice.len;
}

/* ------------------------------------------------------------------------------------------------ Memory::read
 */
s64 Memory::read(Bytes outbuffer)
{
	if (pos == len)
		return 0;

	s64 bytes_remaining = len - pos;
	s64 bytes_to_read = (bytes_remaining > outbuffer.len ? outbuffer.len : bytes_remaining);
	mem::copy(outbuffer.ptr, buffer + pos, bytes_to_read);
	pos += bytes_to_read;
	return bytes_to_read;
}

/* ------------------------------------------------------------------------------------------------ Memory::readFrom
 */
s64 Memory::readFrom(s64 pos, Bytes slice)
{
	assert(pos < len);

	s64 bytes_remaining = len - pos;
	s64 bytes_to_read = (bytes_remaining > slice.len ? slice.len : bytes_remaining);
	mem::copy(slice.ptr, buffer + pos, bytes_to_read);
	return bytes_to_read;
}

/* ------------------------------------------------------------------------------------------------ Memory::createRollback
 */
Memory::Rollback Memory::createRollback()
{
	return len;
}

/* ------------------------------------------------------------------------------------------------ Memory::commitRollback
 */
void Memory::commitRollback(Rollback rollback)
{
	len = rollback;
	rewind();
}

/* ------------------------------------------------------------------------------------------------ StaticBuffer::clear
 */
template<size_t N>
void StaticBuffer<N>::clear()
{
	write_pos = read_pos = 0;
}

/* ------------------------------------------------------------------------------------------------ StaticBuffer::rewind
 */
template<size_t N>
void StaticBuffer<N>::rewind()
{
	read_pos = 0;
}

/* ------------------------------------------------------------------------------------------------ StaticBuffer::write
 */
template<size_t N>
s64 StaticBuffer<N>::write(Bytes slice)
{
	if (write_pos == len)
		return 0;

	s64 space_remaining = len - write_pos;
	s64 bytes_to_write = (slice.len > space_remaining ? space_remaining : slice.len);
	mem::copy(buffer + write_pos, slice.ptr, bytes_to_write);
	write_pos += bytes_to_write;
	buffer[write_pos] = 0;
	return bytes_to_write;
}

/* ------------------------------------------------------------------------------------------------ StaticBuffer::read
 */
template<size_t N>
s64 StaticBuffer<N>::read(Bytes slice)
{
	if (read_pos == write_pos)
		return 0;

	s64 space_remaining = write_pos - read_pos;
	s64 bytes_to_read = (slice.len < space_remaining ? slice.len : space_remaining);
	mem::copy(slice.ptr, buffer + read_pos, bytes_to_read);
	read_pos += bytes_to_read;
	return bytes_to_read;
}

} 
