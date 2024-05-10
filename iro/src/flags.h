/* 
 *  Helper for dealing with enum class flags.
 *  
 *  Nothing crazy fancy, just supports up to 64 flags based on a given enum.
 */

#ifndef _iro_flags_h
#define _iro_flags_h

#include "common.h"

namespace iro
{

template<typename T>
struct Flags
{
	typedef u64 FlagType;

	FlagType flags = 0;
	
	static Flags<T> none() { return {}; }
	static Flags<T> all()  { Flags<T> out; out.flags = (FlagType)-1; return out; }

	constexpr Flags<T>() {}

	template<typename... Args>
	constexpr Flags<T>(Args... args)
	{
		(set(args), ...);
	}

	template<typename... Args>
	static constexpr Flags<T> from(T first, Args... args) 
	{ 
		Flags<T> out = {};
		out.set(first);
		(out.set(args), ...);
		return out;
	}

	b8 testAll(Flags<T> x)
	{
		return (flags & x.flags) == x.flags;
	}

	template<T... args>
	b8 testAll()
	{
		return testAll(Flags<T>::from(args...));
	}

	b8 testAny(Flags<T> x)
	{
		return (flags & x.flags);
	}

	template<T... args>
	b8 testAny()
	{
		return testAny(Flags<T>::from(args...));
	}

	b8 test(T x)
	{
		return flags & ((FlagType)1 << (FlagType)x);
	}

	void set(T x)
	{
		flags |= ((FlagType)1 << (FlagType)x);
	}

	void unset(T x)
	{
		flags &= ~((FlagType)1 << (FlagType)x);
	}
};

}

#endif
