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
  using FlagEnumType = T;

  typedef u64 FlagType;

  FlagType flags = 0;
  
  static Flags<T> none() { return {}; }
  static Flags<T> all()  
  { 
    Flags<T> out; 
    out.flags = (FlagType)-1; 
    return out; 
  }

  void clear() { flags = 0; }

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


// define this on a flags typedef and the enum it uses to allow 
// arbitrary use of the enum values like you would normally do in C
#define DefineFlagsOrOp(FlagsT, EnumT) \
  static FlagsT operator | (EnumT lhs, EnumT rhs) \
  { \
    return FlagsT::from(lhs, rhs); \
  } \
  \
  static FlagsT& operator | (FlagsT&& lhs, EnumT rhs) \
  { \
    lhs.set(rhs); \
    return lhs; \
  } \
  \
  static FlagsT operator | (FlagsT& lhs, EnumT rhs) \
  { \
    lhs.set(rhs); \
    return lhs; \
  }

// this doesnt work but maybe it can work some other way ?
// C++ isnt able to infer the type T if you just use the enums plainly
//template<typename T>
//Flags<T> operator | (typename Flags<T>::FlagEnumType lhs, typename Flags<T>::FlagEnumType rhs)
//{
//  return Flags<T>::from(lhs, rhs);
//}
//
//template<typename T>
//Flags<T>& operator | (Flags<T>&& lhs, typename Flags<T>::FlagEnumType rhs)
//{
//  lhs.set(rhs);
//  return lhs;
//}

}



#endif
