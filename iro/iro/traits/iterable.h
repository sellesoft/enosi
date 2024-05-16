/*
 *  Trait defined by containers that may provide an iterator over their arguments.
 *
 *  Come back to waaay later
 */

#ifndef _iro_iterable_h
#define _iro_iterable_h

#include "../common.h"

#include "concepts"

template<typename T>
struct IterableTrait {};

#define DefineIterableT(T, State, F) \
	template<typename X> \
	struct IterableTrait<T<X>> \
	{ \
	 	struct Iterator \
		{ \
			T<X>* self; \
			\
			struct State; \
			\
			inline X* next() F \
		}; \
		\
		static Iterator makeIterator(T<X>* x) { return Iterator(x); } \
	} 

template<typename T, typename V>
concept Iterable = requires(T* x, IterableTrait<T>::Iterator iter)
{
	{ iter.next() } -> std::same_as<V*>;
};

template<typename V, template<typename> typename T>
	requires Iterable<T<V>, V>
IterableTrait<T<V>>::Iterator newIterator(T<V>& x) 
{
	return IterableTrait<T<V>>::makeIterator(&x);
}

template<typename V, template<typename> typename T>
	requires Iterable<T<V>, V>
inline V* next(T<V>* x)
{
	return IterableTrait<T<V>>::next(x);
}

template<template<class> class T, typename V>
concept Iterator = requires()
{
	IterableTrait<T<V>>::Iterator;
};


#endif // _iro_iterable_h
