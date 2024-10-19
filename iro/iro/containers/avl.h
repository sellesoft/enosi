/*
 *  AVL tree implementation.
 *
 *  TODO(sushi) it would be nice to be able to allocate the stored type 
 *              with the node, rather than always forcing that data to be
 *              stored elsewhere, as this forces using two Pools in most cases.
 *              Problem is that data moves around then, which is normally 
 *              undesirable.
 */

#ifndef _iro_avl_h
#define _iro_avl_h

#include "../common.h"
#include "pool.h"

namespace iro
{

/* ============================================================================
 */
template<

  // the type this tree points at
  typename T,

  // key accessor
  // which could be a hash function or member or whatever
  u64 (*GetKey)(const T*)
  
>
struct AVL
{
  typedef AVL<T, GetKey> Self;

  /* ==========================================================================
  */
  struct Node
  {
    s8 balance_factor = 0; // -1, 0, or 1
    // TODO(sushi) we may be able to do something neat where we instead store 
    //             offsets from the nodes position in the pool with s32s or 
    //             even like s16 to save some space so experiment later
    //             this would require the pools chunks being connected via 
    //             doubly connected list
    Node* left = nullptr;
    Node* right = nullptr;
    Node* parent = nullptr;
    T* data = nullptr;
  };
  
  Node* root;
  Pool<Node> pool;


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */


  /* --------------------------------------------------------------------------
   */
  static Self create(mem::Allocator* allocator = &mem::stl_allocator)
  {
    Self out = {};
    out.init(allocator);
    return out;
  }

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
   */
  b8 has(T* data) const
  {
    if (!root)
      return false;

    u64 data_key = GetKey(data);

    Node* search_node = root;
    for (;;)
    {
      u64 search_key = GetKey(search_node->data);

      if (search_key == data_key)
        return true;

      if (search_key > data_key)
      {
        if (search_node->left)
          search_node = search_node->left;
        else
          return false;
      }
      else if (search_node->right)
        search_node = search_node->right;
      else
        return false;
    }
  }

  /* --------------------------------------------------------------------------
   */
  T* find(u64 key) const
  {
    if (!root)
      return nullptr;

    Node* search_node = root;
    for (;;)
    {
      u64 search_key = GetKey(search_node->data);

      if (search_key == key)
        return search_node->data;

      if (search_key > key)
      {
        if (search_node->left)
          search_node = search_node->left;
        else
          return nullptr;
      }
      else if (search_node->right)
        search_node = search_node->right;
      else
        return nullptr;
    }
  }

  /* --------------------------------------------------------------------------
   */
  Node* insert(u64 data_key)
  {
    if (!root)
    {
      root = pool.add();
      root->balance_factor = 0;
      return root;
    }

    Node* search_node = root;
    b8 place_right = true;
    for (;;)
    {
      u64 search_key = GetKey(search_node->data);

      if (search_key == data_key)
        return search_node;
      
      if (search_key > data_key)
      {
        if (search_node->left)
        {
          search_node = search_node->left;
        }
        else
        {
          place_right = false;
          break;
        }
      }
      else if (search_node->right)
        search_node = search_node->right;
      else
        break; 
    }

    // need a new node
    Node* new_node = pool.add();
    new_node->parent = search_node;
    if (place_right)
      search_node->right = new_node;
    else
      search_node->left = new_node;

    // walk back up the tree and adjust balance
    for (Node* parent = search_node,* child = new_node; 
        parent; 
        child = parent, parent = child->parent)
    {
      Node* new_child = nullptr;
      Node* parents_parent = parent->parent;

      if (parent->right == child)
      {
        // right subtree has increased in height
        if (parent->balance_factor > 0)
        {
          // the parent was already right heavy, so its balance factor
          // is now +2
          if (child->balance_factor < 0)
            new_child = rotateRightLeft(parent, child);
          else 
            new_child = rotateLeft(parent, child);
        }
        else
        {
          // the parent is either already balanced or left heavy
          if (parent->balance_factor < 0)
          {
            // if left heavy, then we're now balanced by the
            // addition to the right child.
            parent->balance_factor = 0;
            break;
          }
          // if we were balanced all we need to do is set the parent's 
          // balance factor to 1 and move on
          parent->balance_factor = 1;
          continue;
        }
      }
      else
      {
        if (parent->balance_factor < 0)
        {
          if (child->balance_factor > 0)
            new_child = rotateLeftRight(parent, child);
          else
            new_child = rotateRight(parent, child);
        }
        else
        {
          if (parent->balance_factor > 0)
          {
            parent->balance_factor = 0;
            break;
          }
          parent->balance_factor = -1;
          continue;
        }
      }

      new_child->parent = parents_parent;
      if (parents_parent) 
      {
        if (parent == parents_parent->left)
          parents_parent->left = new_child;
        else
          parents_parent->right = new_child;
      }
      else
        root = new_child;
      break;
    }

    return new_node;
  }


  /* --------------------------------------------------------------------------
   *  Attempts to insert the given data. If the data already exists then 
   *  nothing happens.
   */
  void insert(T* data)
  {
    if (!root)
    {
      root = pool.add();
      root->data = data;
      root->balance_factor = 0;
      return;
    }

    Node* new_node = insert(GetKey(data));
    new_node->data = data;
  }
  
  /* --------------------------------------------------------------------------
   *  Removes the given data from the tree.
   */
  void remove(T* data)
  {
    u64 data_key = GetKey(data);

    // find the node representing the given data
    Node* search_node = root;
    for (;;)
    {
      if (!search_node)
        return;

      u64 search_key = GetKey(search_node->data);

      if (search_key == data_key)
        break;

      if (data_key > search_key)
        search_node = search_node->right;
      else
        search_node = search_node->left;
    }

    Node* target = search_node;

    if (search_node->left && search_node->right)
    {
      target = predecessor(search_node);
      search_node->data = target->data;
    }

    for (Node* parent = target->parent,* child = target; 
         parent; 
         parent = child->parent)
    {
      Node* parents_parent = parent->parent;
      u8 b = 0;

      if (child == parent->left)
      {
        // left subtree decreases
        if (parent->balance_factor > 0)
        {
          // parent was right heavy
          // so we need to rebalance it 
          Node* sibling = parent->right;
          b = sibling->balance_factor;
          if (b < 0)
            child = rotateRightLeft(parent, sibling);
          else
            child = rotateLeft(parent, sibling);
        }
        else
        {
          if (parent->balance_factor == 0)
          {
            parent->balance_factor = 1;
            break;
          }
          child = parent;
          child ->balance_factor = 0;
          continue;
        }
      }
      else
      {
        if (parent->balance_factor < 0)
        {
          Node* sibling = parent->left;
          b = sibling->balance_factor;
          if (b < 0)
            child = rotateLeftRight(parent, sibling);
          else
            child = rotateRight(parent, sibling);
        }
        else
        {
          if (parent->balance_factor == 0)
          {
            parent->balance_factor = -1;
            break;
          }
          child = parent;
          child ->balance_factor = 0;
          continue;
        }
      }

      child ->parent = parents_parent;
      if (parents_parent)
      {
        if (parent == parents_parent->left)
          parents_parent->left = child ;
        else
          parents_parent->right = child ;
      }
      else
        root = child;

      if (!b)
        break;
    }

    Node* child = (target->left ? target->left : target->right);

    if (child)
      child->parent = target->parent;

    if (target->parent)
    {
      if (target == target->parent->left)
        target->parent->left = child;
      else
        target->parent->right = child;
    }

    pool.remove(target);
  }

  /* --------------------------------------------------------------------------
   *  Replaces u with v
   */
  void replace(Node* u, Node* v)
  {
    if (!u->parent)
      root = v;
    else if (u == u->parent->left)
      u->parent->left = v;
    else
      u->parent->right = v;
    if (v)
      v->parent = u->parent;
  }

  /* --------------------------------------------------------------------------
   */
  static Node* successor(Node* n)
  {
    if (n->right)
      return minimum(n->right);
    Node* parent = n->parent;
    while (parent && n == parent->right)
    {
      n = parent;
      parent = parent->parent;
    }
    return parent;
  }

  /* --------------------------------------------------------------------------
   */
  static Node* predecessor(Node* n)
  {
    if (n->left)
      return maximum(n->left);
    Node* parent = n->parent;
    while (parent && n == parent->left)
    {
      n = parent;
      parent = parent->parent;
    }
    return parent;
  }

  /* --------------------------------------------------------------------------
   */ 
  static Node* maximum(Node* n)
  {
    while (n->right)
      n = n->right;
    return n;
  }

  /* --------------------------------------------------------------------------
   */ 
  static Node* minimum(Node* n)
  {
    while (n->left)
      n = n->left;
    return n;
  }

  /* --------------------------------------------------------------------------
   *  Internal helpers
   */
private:

  /* --------------------------------------------------------------------------
   *  (A a (B b g))
   *  ->
   *  (B (A a b) g)
   */
  Node* rotateLeft(Node* A, Node* B)
  {
    Node* b = B->left;
    A->right = b;
    if (b)
      b->parent = A;
    B->left = A;
    A->parent = B;

    if (!B->balance_factor)
    {
      // in if B was balanced, it no longer is
      // this only happens in deletion
      A->balance_factor =  1;
      B->balance_factor = -1;
    }
    else
    {
      // otherwise the tree has been rebalanced.
      A->balance_factor = 
      B->balance_factor = 0;
    }

    return B;
  }

  /* --------------------------------------------------------------------------
   *  (A (B a b) g)
   *  ->
   *  (B a (A b g))
   */
  Node* rotateRight(Node* A, Node* B)
  {
    Node* b = B->right;
    A->left = b;
    if (b)
      b->parent = A;
    B->right = A;
    A->parent = B;

    if (!B->balance_factor)
    {
      A->balance_factor = -1;
      B->balance_factor =  1;
    }
    else
    {
      B->balance_factor = 
      A->balance_factor = 0;
    }

    return B;
  }
  
  /* --------------------------------------------------------------------------
   *  (A a (C (B b c) d))
   *  -> 
   *  (A a (B b (C c d)))
   *  ->
   *  (B (A a b) (C c d))
   */
  Node* rotateRightLeft(Node* A, Node* C)
  {
    Node* B = C->left;
    Node* c = B->right;
    C->left = c;
    if (c)
      c->parent = C;
    B->right = C;
    C->parent = B;
    Node* b = B->left;
    A->right = b;
    if (b)
      b->parent = A;
    B->left = A;
    A->parent = B;

    if (!B->balance_factor)
    {
      A->balance_factor = 0;
      C->balance_factor = 0;
    }
    else if (B->balance_factor > 0)
    {
      A->balance_factor = -1;
      C->balance_factor =  0;
    }
    else
    {
      A->balance_factor =  0;
      C->balance_factor = -1;
    }

    return B;
  }

  /* --------------------------------------------------------------------------
   *  (A (C a (B b c)) d)
   *  ->
   *  (A (B (C a b) c) d)
   *  ->
   *  (B (C a b) (A c d))
   */
  Node* rotateLeftRight(Node* A, Node* C)
  {
    Node* B = C->right;
    Node* b = B->left;
    C->right = b;
    if (b) 
      b->parent = C;
    B->left = C;  
    C->parent = B;
    Node* c = B->right;
    A->left = c;
    if (c)
      c->parent = A;
    B->right = A;
    A->parent = B;

    if (!B->balance_factor)
    {
      A->balance_factor = 0;
      C->balance_factor = 0;
    }
    else if (B->balance_factor > 0)
    {
      A->balance_factor = -1;
      C->balance_factor =  0;
    }
    else
    {
      A->balance_factor =  0;
      C->balance_factor = -1;
    }

    return B;
  }

public:

  /* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *  Iterator for compatibility with C++ ranged for loops 
   *
   *  It is NOT safe to add or remove things from the tree while using this!!
   */
  struct RangeIterator
  {
    Node* current;

    Node* operator++()
    {
      current = successor(current);
      return current;
    }

    b8 operator !=(const RangeIterator& rhs)
    {
      return current != rhs.current;
    }

    T* operator->()
    {
      return current;
    }

    T& operator*()
    {
      return *current->data;
    }
  };

  RangeIterator begin()
  {
    return RangeIterator{ root? minimum(root) : nullptr };
  }

  RangeIterator end()
  {
    return RangeIterator{ nullptr };
  }
};

/* ============================================================================
 *   Basic example of using AVL.
 */
struct StringSet
{
  struct Elem
  {
    u64 hash;
    String s;
  };

  typedef AVL<Elem, [](const Elem* elem) { return elem->hash; }> Map;

  Pool<Elem> pool;
  Map map;

  b8 init(mem::Allocator* allocator = &mem::stl_allocator)
  {
    if (!pool.init(allocator))
      return false;
    if (!map.init(allocator))
    {
      pool.deinit();
      return false;
    }
    return true;
  }

  void deinit()
  {
    pool.deinit();
    map.deinit();
  }

  void add(String s)
  {
    auto elem = Elem{s.hash(), s};
    if (map.find(elem.hash))
      return;

    auto nuelem = pool.add();
    *nuelem = elem;

    map.insert(nuelem);
  }

  b8 has(String s) const
  {
    if (isnil(s))
      return false;
    return map.find(s.hash()) != nullptr;
  }

  struct RangeIterator
  {
    Map::RangeIterator map_iterator;

    Map::Node* operator++()
    {
      return map_iterator.operator++();
    }
    
    b8 operator != (const RangeIterator& rhs)
    {
      return map_iterator != rhs.map_iterator;
    }

    String* operator->()
    {
      return &map_iterator.current->data->s;
    }

    String& operator*()
    {
      return map_iterator.current->data->s;
    }
  };

  RangeIterator begin() { return RangeIterator{map.begin()}; }
  RangeIterator end() { return RangeIterator{map.end()}; }
};

}

template<typename T, u64 (*GetKey)(const T*)>
struct NilValue<iro::AVL<T, GetKey>>
{
  constexpr static iro::AVL<T, GetKey> Value = {};
  inline static bool isNil(const iro::AVL<T, GetKey>& x) 
    { return isnil(x.pool); }
};

#endif
