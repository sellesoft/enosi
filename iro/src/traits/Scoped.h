/*
 *  Scoped trait allowing types to define how they clean themselves up
 *  when they are no longer needed.
 */

#ifndef _iro_Scoped_h
#define _iro_Scoped_h

#include "Nil.h"

template<typename T>
struct ScopedTrait {};

#define DefineScopedTrait(T, F) \
  struct ScopedTrait \
  { \
    inline static void cleanup(T& x) F \
  };

template<typename T>
concept Scopable = requires(T& x)
{
  T::ScopedTrait::cleanup(x);
};

template<Scopable T>
struct Scoped : T
{
  Scoped<T>(T x) { *(T*)this = x; }
  ~Scoped<T>() { ScopedTrait<T>::cleanup(*this); }

  DefineNilTrait(Scoped<T>, nil, T::isNil(x));
};

template<typename T>
Scoped(T& x) -> Scoped<T>;

template<Scopable T>
Scoped<T> scoped(T&& x) { return Scoped<T>(x); }

#endif // _iro_scoped_h
