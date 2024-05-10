/*
 *  Nil value trait declaration and a helper for defining a NilValue for a given type.
 *
 *  This allows us to use the value 'nil' anywhere we want to indicate an invalid 
 *  value of a type that implements NilValue.
 *  
 *  This is currently not defined in the iro namespace because I feel my use of it 
 *  will be common, but I'm not sure if this is a good idea yet.
 */

#ifndef _iro_nil_h
#define _iro_nil_h

/* ================================================================================================ NilValue
 *  Template for types that define a nil value. 
 *	
 *	Types implementing this are expected to define two things:
 *		
 *		constexpr static const T Value;
 *		  	A compile time value that represents the nil state of T.
 *		
 *		static b8 isNil(const T& x);
 *			A function used to check if 'x' is nil.
 *
 *	The helper DefineNilValue helps wrap up the boilerplate needed to actually define
 *	these things.
 *
 */
template<typename T>
struct NilValue {};

/* ------------------------------------------------------------------------------------------------ DefineNilValue
 *  Defines value 'V" as the NilValue for the type 'T'
 *  and defines 'F' as the function to check if an instance 
 *  of 'T' is nil.
 */
#define DefineNilValue(T, V, F)              \
	template<>                               \
	struct NilValue<T>                       \
	{                                        \
		constexpr static const T Value = V;  \
		inline static b8 isNil(const T& x) F \
	}                                        \

/* ------------------------------------------------------------------------------------------------ Nillable
 *  Concept used to require a type to have implemented NilValue and so compiler errors aren't 
 *  so horrible.
 */
template<typename T>
concept Nillable = requires(const T& x)
{
	NilValue<T>::Value;
	NilValue<T>::isNil(x);
};

/* ------------------------------------------------------------------------------------------------ Nil
 *  The Nil type which implicitly converts to any type as long as it implements NilValue.
 */
struct Nil
{
	template<Nillable T>
	consteval operator T() const { return NilValue<T>::Value; }

	// any pointer can be nil!
	// note this also handles the case of checking if a pointer is nil
	template<Nillable T>
	consteval operator T*() const { return nullptr; }

	template<Nillable T>
	inline bool operator == (const T& rhs) const { return NilValue<T>::isNil(rhs); }
};

/* ------------------------------------------------------------------------------------------------ nil
 *  And finally, *the* nil value.
 */
constexpr Nil nil = Nil();

/* ------------------------------------------------------------------------------------------------ Common nil value definitions
 *  Some nil value definitions for the common types.
 */
#include "common.h"

#define DefineTrivialNilValue(T, V) DefineNilValue(T, V, { return x == Value; })

DefineTrivialNilValue(u8,   0);
DefineTrivialNilValue(u16,  0);
DefineTrivialNilValue(u32,  0);
DefineTrivialNilValue(u64,  0);
DefineTrivialNilValue(s8,   0);
DefineTrivialNilValue(s16,  0);
DefineTrivialNilValue(s32,  0);
DefineTrivialNilValue(s64,  0);
DefineTrivialNilValue(f32,  0.f);
DefineTrivialNilValue(f64,  0.f);
DefineTrivialNilValue(void*, nullptr);

#undef DefineTrivialNilValue

#endif
