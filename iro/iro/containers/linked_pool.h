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


template<typename T>
struct DLinkedPool
{
	Pool<T> pool;
	DList<T> list;

	typedef DList<T>::Node Node;

	static DLinkedPool<T> create(mem::Allocator* allocator = &mem::stl_allocator)
	{
		DLinkedPool<T> out;
		out.pool = Pool<T>::create(allocator);
		out.list = DList<T>::create(allocator);
		return out;
	}

	void destroy()
	{
		pool.destroy();
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
		assert(not isEmpty());
		return list.head;
	}

	Node* tailNode()
	{
		assert(not isEmpty());
		return list.tail;
	}	
};

}

#endif // _iro_linked_pool_h
