/*
 *  Container implementing a Pool that links each element.
 *
 *  TODO(sushi) the linking should be specialized to be a header on each element
 *              rather than literally using DList to store links. I'm too lazy
 *              to implement that atm though cause i just want this fucking 
 *              globbing thing to work already.
 */

#ifndef _iro_linked_pool_h
#define _iro_linked_pool_h

#include "pool.h"
#include "list.h"

namespace iro
{

// NOT finished cause i started adding it for something then realized that i needed
// DLinkedPool instead sooooo
template<typename T>
struct SLinkedPool
{
  Pool<T> pool;
  SList<T> list;

  typedef SList<T> List;
  typedef List::Node Node;

  b8 init(mem::Allocator* allocator = &mem::stl_allocator)
  {
    pool = Pool<T>::create(allocator);
    list = SList<T>::create(allocator);
    return true;
  }

  void deinit()
  {
    pool.deinit();
    list.destroy();
  }

  b8 isEmpty()
  {
    return list.isEmpty();
  }

  Node* push()
  {
    return list.push(pool.add());
  }

  T pop()
  {
    Node* n = list.head;
    T* ptr = list.pop();
    T out = *ptr;
    pool.remove(ptr);
    return out;
  }

  T& head()
  {
    assert(not isEmpty());
    return *list.head->data;
  }

  Node* headNode()
  {
    return list.head;
  }

  List::RangeIterator begin() { return list.begin(); }
  List::RangeIterator end() { return list.end(); }
};

// TODO(sushi) update to init/deinit
template<typename T>
struct DLinkedPool
{
  Pool<T> pool;
  DList<T> list;

  typedef DList<T> List;
  typedef List::Node Node;

  static DLinkedPool<T> create(mem::Allocator* allocator = &mem::stl_allocator)
  {
    DLinkedPool<T> out;
    out.init(allocator);
    return out;
  }

  b8 init(mem::Allocator* allocator = &mem::stl_allocator)
  {
    pool = Pool<T>::create(allocator);
    list = DList<T>::create(allocator);
    return true;
  }

  void deinit()
  {
    pool.deinit();
    list.destroy();
  }

  b8 isEmpty()
  {
    return list.head == nullptr;
  }

  Node* pushHead()
  {
    return list.pushHead(pool.add());
  }

  void pushHead(const T& v)
  {
    *pushHead()->data = v;
  }

  void pushTail(const T& v)
  {
    *pushTail()->data = v;
  }

  Node* pushTail()
  {
    return list.pushTail(pool.add());
  }

  T popHead()
  {
    Node* h = list.head;
    T out = *h->data;
    remove(h);
    return out;
  }

  T popTail()
  {
    Node* t = list.tail;
    T out = *t->data;
    remove(t);
    return out;
  }

  void unlink(Node* n)
  {
    list.remove(n);
  }

  void remove(Node* n)
  {
    list.destroy(n);
  }

  T& head()
  {
    assert(not isEmpty());
    return *list.head->data;
  }

  T& tail()
  {
    assert(not isEmpty());
    return *list.tail->data;
  }

  Node* headNode()
  {
    return list.head;
  }

  Node* tailNode()
  {
    return list.tail;
  }

  List::RangeIterator begin() { return list.begin(); }
  List::RangeIterator end() { return list.end(); }
};

}

#endif // _iro_linked_pool_h
