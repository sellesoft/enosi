/*
 *  TODO(sushi) update this explanation once i feel like it idk
 *
 *  Nil value trait declaration and a helper for defining a NilValue for a 
 *  given type.
 *
 *  This allows us to use the value 'nil' anywhere we want to indicate an 
 *  invalid value of a type that implements NilValue.
 *
 *  When an object is 'nil' it is in some state where it is useless and, most 
 *  importantly, in a state where it does not own resources.
 *  
 *  This is currently not defined in the iro namespace because I feel my use of 
 *  it will be common, but I'm not sure if this is a good idea yet.
 */

#ifndef _iro_Nil_h
#define _iro_Nil_h

#include "../Common.h"

/* ----------------------------------------------------------------------------
 *  Defines value 'V" as the NilValue for the type 'T'
 *  and defines 'F' as the function to check if an instance 
 *  of 'T' is nil.
 */
#define DefineNilTrait(T, V, F) \
  struct NilTrait \
  { \
    static constexpr T getValue() \
    { \
      return V; \
    } \
    \
    static b8 isNil(const T& x) \
    { \
      return F; \
    } \
  };

/* ----------------------------------------------------------------------------
 *  Concept used to require a type to have implemented NilValue and so compiler 
 *  errors aren't so horrible.
 */
template<typename T>
concept Nillable = requires(const T& x)
{
  T::NilTrait::getValue();
  T::NilTrait::isNil(x);
};

/* ----------------------------------------------------------------------------
 *  The Nil type which implicitly converts to any type as long as it implements 
 *  NilValue.
 */
struct Nil
{
  constexpr Nil() {}
  constexpr ~Nil() {}

  template<Nillable T>
  constexpr operator T() const { return T::NilTrait::getValue(); }

  // any pointer can be nil!
  // note this also handles the case of checking if a pointer is nil
  template<Nillable T>
  constexpr operator T*() const { return nullptr; }
};

template<Nillable T>
release_inline bool isnil(const T& x) { return T::NilTrait::isNil(x); }

template<Nillable T>
release_inline bool notnil(const T& x) { return not isnil(x); }

#define COALESCE(x, v) (notnil(x) ? (x) : (v))

// 'resolves' a nillable value 'x' to 'v' if x is nil, otherwise does nothing.
// Returns true if the value was resolved.
template<Nillable T>
inline b8 resolve(T& x, T v) 
{
  if (isnil(x))
  {
    x = v;
    return true;
  }
  return false;
}

template<Nillable T>
inline T resolved(T x, T v)
{
  return isnil(x)? v : x;
}

/* ----------------------------------------------------------------------------
 *  And finally, *the* nil value.
 */
constexpr Nil nil = Nil();

/* ----------------------------------------------------------------------------
 *  Type that indicates a value may either be nil or a non-nil value of type T.
 */
// This was an attempt at getting usage of things that can be nil to be more explicit, but C++ seems
// to not be friendly to what I wanted to do.
//
// Maybe I'll return to this later if the plain nil usage seems to be too confusing.
//
//template<typename T>
//struct NilOr
//{
//  T x;
//
//  consteval NilOr<T>(const Nil& n) : x(n) {}
//  NilOr<T>(T x) : T(x) {}
//
//  explicit operator T() { assert(notnil() || !"attempt to coerce a nil value to its underlying type"); return *this;  }
//
//  inline bool isnil() { return isnil(*this); }
//  inline bool notnil() { return notnil(*this); } 
//};
//
// template<typename T>
// struct NilValue<NilOr<T>>
// {
//  constexpr static const T Value = NilValue<T>::Value;
//  inline static bool isNil(const NilOr<T>& x) { return NilValue<T>::isNil(x); }
// };

#endif
