/*
 *  Basic move semantics.
 *
 *  I don't know if this will be useful or even work well, 
 *  just wanna experiment with it.
 */

#ifndef _iro_move_h
#define _iro_move_h

#include "nil.h"

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
 *  object passed into that function is expected to be moved.
 *
 *  Moving an object means to transfer ownership of data that it owns to some other object.
 *  The original owner is then nil'd to indicate that it no longer owns anything.
 */
template<Nillable T>
struct Moved : public T {};

template<Movable T>
void move(T& from, T& to)
{
	MoveTrait<T>::doMove(from, to);
	from = nil;
}

template<Movable T>
Moved<T> move(T&& from) // NOTE(sushi) this allows moving temp values, like move(Path::from("hi"_str))
{
	Moved<T> to = {};
	move(from, (T&)to);
	return to;
}

template<Movable T>
Moved<T> move(T& from)
{
	Moved<T> to = {};
	move(from, (T&)to);
	return to;
}

// Moved values decay to their underlying values when dealing with nil.
template<Nillable T>
struct NilValue<Moved<T>>
{
	constexpr static const T Value = NilValue<T>::Value;
	inline static bool isNil(const Moved<T>& x) { return NilValue<T>::isNil(x); }
};

#endif

