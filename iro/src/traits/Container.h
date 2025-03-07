/*
 *  Container trait for generic placement of things into other things, 
 *  removing things, and getting things.
 */

#ifndef _iro_Container_h
#define _iro_Container_h

#include "../Common.h"

#include "Nil.h"

#include "assert.h"
#include "concepts"


/* ================================================================================================ ExpandableContainerTrait
 *  Trait applied to containers that can have items added to them.
 *
 *  Must define an 'add' that takes the value to add and returns true if the value was added 
 *  successfully or false if it couldn't be added.
 */
template<typename T>
struct ExpandableContainerTrait {};

#define DefineExpandableContainerT(T, F) \
  template<typename A> \
  struct ExpandableContainerTrait<T<A>> \
  { \
    inline static b8 add(T<A>* self, A& value) F \
  } \

template<typename T, typename V>
concept ExpandableContainer = requires(T* x, V& v)
{
  { ExpandableContainerTrait<T>::add(x, v) } -> std::same_as<b8>;
};

template<typename T, typename V>
  requires ExpandableContainer<T, V> && Nillable<T>
inline b8 containerAdd(T* x, V&& v) 
{ 
  assert(x && notnil(*x) && "attempt to add a value to a nil container!"); 
  return ExpandableContainerTrait<T>::add(x, v); 
}

template<typename T, typename V>
  requires ExpandableContainer<T, V>
inline b8 containerAdd(T* x, V&& v) 
{ 
  assert(x && "container passed to containerAdd is null"); 
  return ExpandableContainerTrait<T>::add(x, v); 
}


/* ================================================================================================ ReducibleContainerTrait
 *  Trait applied to containers that can have items removed from them.
 *
 *  The item to remove is passed as a pointer.
 *
 *  TODO(sushi) implement this along with IndexableContainer and FullContainer if they're ever
 *              useful and if trying to mix them doesnt cause insane chaos.
 */
template<typename T>
struct ReducibleContainerTrait {};

#define DefineReducibleContainerT(T, F) \
  template<typename A> \
  struct ReducibleContainerTrait<T<A>> \
  { \
    inline static b8 remove(T<A>* self, A* x) F \
  } 

template<typename T, typename V>
concept ReducibleContainer = requires(T* x, V* v) 
{
  { ReducibleContainerTrait<T>::remove(x, v) } -> std::same_as<b8>;
};

template<typename T, typename V>
  requires ReducibleContainer<T, V>
inline b8 containerRemove(T* x, V* v) 
{ 
  assert(x && "container passed to containerRemove is null");
  return ReducibleContainerTrait<T>::remove(x, v);
}

/* ================================================================================================ IndexableContainerTrait
 *  Trait applied to containers that can be indexed by an arbitrary number.
 *
 *  Must define 'index' which returns a pointer to the element at that index and nullptr if one 
 *  does not exist there.
 */
template<typename T>
struct IndexableContainerTrait {};

#define DefineIndexableContainerT(T, F) \
  template<typename A> \
  struct IndexableContainerTrait<T<A>> \
  { \
    inline static A* index(T<A>* self, u64 idx) F \
  }

template<typename T, typename V>
concept IndexableContainer = requires(T* x, u64 idx)
{
  { IndexableContainerTrait<T>::index(x, idx) } -> std::same_as<V*>;
};

template<typename T, typename V>
  requires IndexableContainer<T, V>
inline V* containerIndex(T* x, u64 idx) { assert(x && "containerIndex passed a null container"); return IndexableContainerTrait<T>::index(x, idx); }


// mix of an indexable container with one that can remove items
template<template<typename> typename T, typename V>
  requires IndexableContainer<T<V>, V> && 
         ReducibleContainer<T<V>, V>
inline b8 containerRemoveAtIdx(T<V>* x, u64 idx)
{
  return containerRemove<T<V>, V>(x, containerIndex<T<V>, V>(x, idx));
}


#endif
