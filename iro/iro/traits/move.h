/*
 *  Basic move semantics.
 *
 *  I don't know if this will be useful or even work well, 
 *  just wanna experiment with it.
 */

#ifndef _iro_move_h
#define _iro_move_h

#include "nil.h"
#include "assert.h"

template<typename T>
struct MoveTrait {};

template<typename T>
concept Movable = Nillable<T> && requires(T& from, T& to)
{
	MoveTrait<T>::doMove(from, to);
};

#define DefineMove(T, F) \
	template<> \
	struct MoveTrait<T> \
	{ \
		inline static void doMove(T& from, T& to) F \
	} 

/* ================================================================================================ Moved
 *  Contains a value that has been moved. Eg. if this is the type of a function parameter, the 
 *  object passed into that function is the new owner of whatever data that type owns.
 *
 *  Moving an object means to transfer ownership of data that it owns to some other object.
 *  The original owner is then nil'd to indicate that it no longer owns anything.
 */
template<Movable T>
struct Moved : public T {};

template<Movable T>
inline void move(T& from, T& to)
{
	assert(notnil(from) && "attempt to move a nil value");
	MoveTrait<T>::doMove(from, to);
	from = nil;
}

template<Movable T>
inline Moved<T> move(T& from)
{
	Moved<T> to = {};
	move(from, (T&)to);
	return to;
}

template<Movable T>
inline Moved<T> move(T&& from) // NOTE(sushi) this allows moving temp values, like move(Path::from("hi"_str))
{
	return move((T&)from);
}

// Moved values decay to their underlying values when dealing with nil.
template<Nillable T>
struct NilValue<Moved<T>>
{
	constexpr static const T Value = NilValue<T>::Value;
	inline static bool isNil(const Moved<T>& x) { return NilValue<T>::isNil(x); }
};

/* ================================================================================================ MayMove
 *  Wraps an object that may be moved by someone. 
 */
template<Movable T>
struct MayMove
{
	T x;

	// By default initialize to the wrapped type's nil value.
	MayMove<T>() : x(nil) {}

	// Use to set this with the value of something that may be moved.
	MayMove<T>(const T& x) { *this = x; };

	MayMove<T>& operator =(const T& x) { assert(wasMoved()); this->x = x; return *this; }

	// Doesn't make sense to copy this, because the value its wrapping.
	// may be moved.
	MayMove<T>(const MayMove<T>& x) = delete;

	T* operator ->() { assert(not wasMoved()); return &x; }
	Moved<T> operator *() { return move(); }

	inline b8 wasMoved() { return isnil(x); }
	inline Moved<T> move() { return assert(not wasMoved()); ::move(x); }
};

template<Movable T>
struct NilValue<MayMove<T>>
{
	constexpr static const T Value = NilValue<T>::Value;
	inline static bool isNil(const MayMove<T>& x) { return NilValue<T>::isNil((const T&)x); }
};

#endif

