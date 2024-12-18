/*
 *  Pair util.
 */

#ifndef _iro_pair_h
#define _iro_pair_h

template<typename A, typename B>
struct Pair
{
  A first;
  B second;

  Pair() { first = {}; second = {}; }
  Pair(A first, B second) : first(first), second(second) { }
};

#endif
