/*
 *  Scoped trait allowing types to define how they clean themselves up
 *  when they are no longer needed.
 */

#ifndef _iro_scoped_h
#define _iro_scoped_h

#include "nil.h"

template<typename T>
struct ScopedTrait {};

#define DefineScoped(T, F) \
	template<> \
	struct ScopedTrait<T> \
	{ \
		inline static void cleanup(T& x) F \
	}

template<typename T>
concept Scopable = requires(T& x)
{
	ScopedTrait<T>::cleanup(x);
};

template<Scopable T>
struct Scoped : T
{
	Scoped<T>(T x) { *(T*)this = x; }
	~Scoped<T>() { ScopedTrait<T>::cleanup(*this); }
};

template<typename T>
Scoped(T& x) -> Scoped<T>;

template<Scopable T>
Scoped<T> scoped(T&& x) { return Scoped<T>(x); }

template<typename T>
struct NilValue<Scoped<T>>
{
	constexpr static const T Value = NilValue<T>::Value;
	inline static bool isNil(const Scoped<T>& x) { return NilValue<T>::isNil(x); }
};

#endif // _iro_scoped_h
