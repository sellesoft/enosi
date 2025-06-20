/*
 *  Generic linked list structures
 */

#ifndef _iro_List_h
#define _iro_List_h

#include "Pool.h"

namespace iro
{

/* ================================================================================================ SList
 *  Singly linked list.
 */
template<typename T>
struct SList
{
  /* ============================================================================================ SList::Node
   */
  struct Node
  {
    T*    data = nullptr;
    Node* next = nullptr;

    operator T&() { return *data; }
  };

  Pool<Node> pool = {};

  Node* head = nullptr;


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */


  /* -------------------------------------------------------------------------------------------- create
   */
  static SList<T> create(mem::Allocator* allocator = &mem::stl_allocator)
  {
    SList<T> out = {};
    out.init(allocator);
    return out;
  }

  /* -------------------------------------------------------------------------------------------- destroy
   */
  b8 init(mem::Allocator* allocator = &mem::stl_allocator)
  {
    if (!pool.init(allocator))
      return false;
    head = nullptr;
    return true;
  }

  /* -------------------------------------------------------------------------------------------- destroy
   */
  void deinit()
  {
    pool.deinit();
    head = nullptr;
  }

  /* -------------------------------------------------------------------------------------------- push
   */
  Node* push(T* x)
  {
    Node* n = pool.add();
    n->next = head;
    n->data = x;
    head = n;
    return n;
  }

  /* -------------------------------------------------------------------------------------------- pop
   */
  T* pop()
  {
    Node* n = head;
    head = n->next;

    T* data = n->data;
    pool.remove(n);
    return data;
  }

  /* -------------------------------------------------------------------------------------------- removeNext
   *  Removes the node following the one given from the list
   */
  void removeNext(Node* n)
  {
    Node* save = n->next;
    n->next = save->next;
    pool.remove(n);
  } 

  /* -------------------------------------------------------------------------------------------- isEmpty
   */
  b8 isEmpty()
  {
    return !head;
  }

  /* -------------------------------------------------------------------------------------------- copy
   *  Returns a copy of this list.
   */
  SList<T> copy()
  {
    SList<T> out = SList<T>::create();

    if (!head)
      return out;

    Node* old_iter = head;
    Node* new_iter = out.push(head->data);
    
    while (old_iter->next)
    {
      old_iter = old_iter->next;
      Node* n = out.pool.add();
      n->data = old_iter->data;
      new_iter->next = n;
      new_iter = n;
    }

    return out;
  }

  /* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ RangeIterator 
   *  Iterator for compatibility with C++ ranged for loops 
   */
  struct RangeIterator
  {
    Node* n;

    Node* operator++()
    {
      n = n->next;
      return n;
    }

    b8 operator !=(const RangeIterator& rhs)
    {
      return n != rhs.n;
    }

    T* operator->()
    {
      return n->data;
    }

    T& operator*()
    {
      return *n->data;
    }
  };


  RangeIterator begin() 
  {
    return RangeIterator{head};
  }

  RangeIterator end() 
  {
    return RangeIterator{nullptr};
  }

  DefineNilTrait(SList<T>, {nil}, isnil(x.pool));
};

/* ================================================================================================ DList
 *  Doubly linked list
 */
template<typename T>
struct DList
{

  /* ============================================================================================ DList::Node
  */
  struct Node 
  {
    T*    data = nullptr;
    Node* next = nullptr;
    Node* prev = nullptr;
  };

  Pool<Node> pool;

  Node* head;
  Node* tail;
  

  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */


  /* -------------------------------------------------------------------------------------------- create
   */
  static DList<T> create(mem::Allocator* allocator = &mem::stl_allocator)
  {
    DList<T> out = {};
    out.init(allocator);
    return out;
  }

  /* -------------------------------------------------------------------------------------------- create
   */
  b8 init(mem::Allocator* allocator = &mem::stl_allocator)
  {
    if (!pool.init(allocator))
      return false;
    head = tail = nullptr;
    return true;
  }

  /* -------------------------------------------------------------------------------------------- create
   */
  void deinit()
  {
    pool.deinit();
    head = tail = nullptr;
  }

  /* -------------------------------------------------------------------------------------------- front
   */
  T* front()
  {
    return head? head->data : nullptr;
  }

  /* -------------------------------------------------------------------------------------------- back
   */
  T* back()
  {
    return tail? tail->data : nullptr;
  }

  /* -------------------------------------------------------------------------------------------- isEmpty
   */
  b8 isEmpty()
  {
    return !head;
  }

  /* -------------------------------------------------------------------------------------------- insertAfter
   */
  Node* insertAfter(Node* x, T* t = nullptr)
  {
    Node* n = pool.add();
    n->prev = x;
    if (x->next) 
    {
      x->next->prev = n;
      n->next = x->next;
    }
    else
      tail = n;
    x->next = n;
    n->data = t;
    return n;
  }

  /* -------------------------------------------------------------------------------------------- insertBefore
   */
  Node* insertBefore(Node* x, T* t = nullptr)
  {
    Node* n = pool.add();
    n->next = x;
    if (x->prev)
    {
      x->prev->next = n;
      n->prev = x->prev;
    }
    else
      head = n;
    x->prev = n;
    n->data = t;
    return n;
  }

  /* -------------------------------------------------------------------------------------------- pushHead
   */
  Node* pushHead(T* x = nullptr)
  {
    if (head)
      return insertBefore(head, x);

    head = pool.add();
    tail = head;
    head->data = x;
    return head;
  }

  /* -------------------------------------------------------------------------------------------- pushTail
   */
  Node* pushTail(T* x = nullptr)
  {
    if (tail)
      return insertAfter(tail, x);
    return pushHead(x);
  }

  /* -------------------------------------------------------------------------------------------- remove
   */
  void remove(Node* x)
  {
    if (x->prev)
      x->prev->next = x->next;
    else
      head = x->next;

    if (x->next)
      x->next->prev = x->prev;
    else
      tail = x->prev;
  } 

  /* -------------------------------------------------------------------------------------------- destroy
   *  removes the given node from the list and also deletes it from the pool
   */
  void destroy(Node* x)
  {
    remove(x);
    pool.remove(x);
  }

  /* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ RangeIterator 
   *  Iterator for compatibility with C++ ranged for loops 
   */
  struct RangeIterator
  {
    Node* n;

    Node* operator++()
    {
      n = n->next;
      return n;
    }

    b8 operator !=(const RangeIterator& rhs)
    {
      return n != rhs.n;
    }

    T* operator->()
    {
      return n->data;
    }

    T& operator*()
    {
      return *n->data;
    }
  };

  RangeIterator begin() const
  {
    return RangeIterator{head};
  }

  RangeIterator end() const
  {
    return RangeIterator{nullptr};
  }

  // disgusting, sorry, idk if there's a nicer way to do this yet.
  struct RangeIteratorPtr
  {
    Node* start;

    struct ActualIterator
    {
      Node* n;

      Node* operator++()
      {
        n = n->next;
        return n;
      }

      b8 operator !=(const ActualIterator& rhs)
      {
        return n != rhs.n;
      }

      T* operator->()
      {
        return n->data;
      }

      T* operator*()
      {
        return n->data;
      }
    };

    ActualIterator begin() const { return ActualIterator{start}; }
    ActualIterator end() const { return ActualIterator{nullptr}; }
  };

  RangeIteratorPtr eachPtr() const { return RangeIteratorPtr{head}; }
};


/* ================================================================================================ DListIterator  
 */
template<typename T>
struct DListIterator
{
  DList<T>::Node* current;

  
  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */


  /* -------------------------------------------------------------------------------------------- DListIterator<T>()
   */
  DListIterator<T>(DList<T>& list) 
  {
    current = list.head;
  }

  /* -------------------------------------------------------------------------------------------- DListIterator<T>()
   */
  T* next()
  {
    if (!current)
      return nullptr;

    T* out = current->data;
    current = current->next;
    return out;
  }

  /* -------------------------------------------------------------------------------------------- operator->
   */
  T* operator->()
  {
    return current->data;
  }


  /* -------------------------------------------------------------------------------------------- operator bool
   */
  operator bool()
  {
    return current;
  }
};

}

#endif
