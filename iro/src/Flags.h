/* 
 *  Helper for dealing with enum class flags.
 *  
 *  Nothing crazy fancy, just supports up to 64 flags based on a given enum.
 */

#ifndef _iro_flags_h
#define _iro_flags_h

#include "Common.h"

namespace iro
{

template<u64 bytes> struct FlagTypeSelector;
template<> struct FlagTypeSelector<0> {};
template<> struct FlagTypeSelector<1> { typedef u8  Type; };
template<> struct FlagTypeSelector<2> { typedef u16 Type; };
template<> struct FlagTypeSelector<3> { typedef u32 Type; };
template<> struct FlagTypeSelector<4> { typedef u32 Type; };
template<> struct FlagTypeSelector<5> { typedef u64 Type; };
template<> struct FlagTypeSelector<6> { typedef u64 Type; };
template<> struct FlagTypeSelector<7> { typedef u64 Type; };
template<> struct FlagTypeSelector<8> { typedef u64 Type; };

template<typename T>
struct Flags
{
  using FlagEnumType = T;

  typedef FlagTypeSelector<(u32(T::COUNT) + 7) / 8>::Type FlagType;

  FlagType flags = 0;
  
  static constexpr Flags<T> none() { return {}; }
  static constexpr Flags<T> all()  
  { 
    Flags<T> out; 
    out.flags = (FlagType)-1; 
    return out; 
  }

  constexpr void clear() { flags = 0; }

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

  constexpr b8 testAll(Flags<T> x) const
  {
    return (flags & x.flags) == x.flags;
  }

  template<T... args>
  constexpr b8 testAll() const
  {
    return testAll(Flags<T>::from(args...));
  }

  constexpr b8 testAny(Flags<T> x) const
  {
    return 0 != (flags & x.flags);
  }

  template<T... args>
  constexpr b8 testAny() const
  {
    return testAny(Flags<T>::from(args...));
  }

  constexpr b8 test(T x) const
  {
    return 0 != (flags & ((FlagType)1 << (FlagType)x));
  }

  constexpr void set(T x)
  {
    flags |= ((FlagType)1 << (FlagType)x);
  }

  constexpr void unset(T x)
  {
    flags &= ~((FlagType)1 << (FlagType)x);
  }

  b8 isNone() const
  {
    return flags == 0;
  }
};


// define this on a flags typedef and the enum it uses to allow 
// arbitrary use of the enum values like you would normally do in C
#define DefineFlagsOrOp(FlagsT, EnumT) \
  static constexpr FlagsT operator | (EnumT lhs, EnumT rhs) \
  { \
    return FlagsT::from(lhs, rhs); \
  } \
  \
  static constexpr FlagsT& operator | (FlagsT&& lhs, EnumT rhs) \
  { \
    lhs.set(rhs); \
    return lhs; \
  } \
  \
  static constexpr FlagsT operator | (FlagsT& lhs, EnumT rhs) \
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
