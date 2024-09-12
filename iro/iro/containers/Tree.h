/*
 *  
 */

#ifndef _iro_tree_h
#define _iro_tree_h

#include "../common.h"
#include "pool.h"

namespace iro
{

template<typename T>
struct Tree
{
  struct Node
  {
    T* data = nullptr;
    Node* next = nullptr;
    Node* prev = nullptr;
    Node* parent = nullptr;
    Node* first_child = nullptr;
    Node* last_child = nullptr;
    u32 child_count = 0;

    /* ------------------------------------------------------------------------
     *  Shorthand for retrieving data ptr.
     */
    T* operator*()
    {
      return data;
    }

    /* ------------------------------------------------------------------------
     *  Inserts this node after 'x'.
     */
    void insertAfter(Node* x)
    {
      if (x->next)
        x->next->prev = this;
      next = x->next;
      prev = x;
      x->next = this;
    }

    /* ------------------------------------------------------------------------
     *  Inserts this node before 'x'.
     */
    void insertBefore(Node* x)
    {
      if (x->prev)
        x->prev->next = this;
      prev = x->prev;
      next = x;
      x->prev = this;
    }

    /* ------------------------------------------------------------------------
     *  Removes this node from a sibling list.
     */
    void removeHorizontally()
    {
      if (next) next->prev = prev;
      if (prev) prev->next = next;
      next = prev = nullptr;
    }

    /* ------------------------------------------------------------------------
     *  Inserts this node as the last child of 'x'.
     */
    void insertLast(Node* x)
    {
      if (x == nullptr)
      {
        parent = nullptr;
        return;
      }

      parent = x;
      if (parent->first_child)
      {
        // If the parent has children just insert after its last child.
        insertAfter(parent->last_child);
        parent->last_child = this;
      }
      else
      {
        // Otherwise init the parents first child to this.
        parent->first_child = parent->last_child = this;
      }

      parent->child_count += 1;
    }

    /* ------------------------------------------------------------------------
     *  Inserts this node as the first child of 'x'.
     */
    void insertFirst(Node* x)
    {
      if (x == nullptr)
      {
        parent = nullptr;
        return;
      }

      parent = x;
      if (parent->first_child)
      {
        // If the parent has children just insert before its first child.
        insertBefore(parent->first_child);
        parent->first_child = this;
      }
      else
      {
        // Otherwise init the parents first child to this.
        parent->first_child = parent->last_child = this;
      }

      parent->child_count += 1;
    }

    /* ------------------------------------------------------------------------
     *  Changes the parent of this node to 'x'.
     */
    void changeParent(Node* x)
    {
      if (parent)
      {
        if (parent->child_count > 1)
        {
          if (this == parent->first_child)
            parent->first_child = next;
          else if (this == parent->last_child)
            parent->last_child = prev;
        }
        else
        {
          parent->first_child = parent->last_child = nullptr;
        }
        parent->child_count -= 1;
      }

      removeHorizontally();

      insertLast(x);
    }
  };

  Pool<Node> pool;

  Node* root = nullptr;

  /* --------------------------------------------------------------------------
   */
  b8 init(mem::Allocator* allocator = &mem::stl_allocator)
  {
    if (!pool.init(allocator))
      return false;
    root = nullptr;
    return true;
  }

  /* --------------------------------------------------------------------------
   */
  void deinit()
  {
    pool.deinit();
    root = nullptr;
  }

  /* --------------------------------------------------------------------------
   *  Adds a new node to the tree without connecting it to anything. If this 
   *  is the first node, it is set as root.
   */
  Node& add(T* data = nullptr)
  {
    Node* nu = pool.add();
    nu->data = data;
    if (!root)
      root = nu;
    return *nu;
  }

  /* --------------------------------------------------------------------------
   *  Adds a new Node after 'x'.
   */
  Node& addAfter(Node* x, T* data = nullptr)
  {
    Node* nu = pool.add();
    nu->data = data;
    nu->insertAfter(x);
    return *nu;
  }

  /* --------------------------------------------------------------------------
   *  Adds a new Node before 'x'.
   */
  Node& addBefore(Node* x, T* data = nullptr)
  {
    Node* nu = pool.add();
    nu->data = data;
    nu->insertBefore(x);
    return *nu;
  }

  /* --------------------------------------------------------------------------
   *  Adds a new Node as the last child of 'x'.
   */
  Node& addLast(Node* x, T* data = nullptr)
  {
    Node* nu = pool.add();
    nu->data = data;
    nu->insertLast(x);
    return *nu;
  }

  /* --------------------------------------------------------------------------
   *  Adds a new Node as the first child of 'x'.
   */
  Node& addFirst(Node* x, T* data = nullptr)
  {
    Node* nu = pool.add();
    nu->data = data;
    nu->insertFirst(x);
    return *nu;
  }

};

}

#endif
