/*
 *
 *  AVL tree implementation
 *	
 *	The tree is specialized to work with unique memory addresses and so does not 
 *	deal with hashing its elements and sorts by the element's address. Its purpose
 *	is primarily for making sure we only store something once.
 *
 */

#ifndef _lake_avl_h
#define _lake_avl_h

#include "common.h"
#include "pool.h"

/* ================================================================================================ AVL
 */
template<typename T>
struct AVL
{
	/* ============================================================================================ AVL::Node
 	*/
	struct Node
	{
		s8 balance_factor; // -1, 0, or 1
		Node* left;
		Node* right;
		Node* parent;
		T* data;
	};
	
	Node* root;

	Pool<Node> pool;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	/* -------------------------------------------------------------------------------------------- create
	 */
	static AVL<T> create()
	{
		AVL<T> out = {};
		out.pool = Pool<Node>::create();
		return out;
	}

	/* -------------------------------------------------------------------------------------------- destroy
	 */
	void destroy()
	{
		pool.destroy();
		root = nullptr;
	}

	/* -------------------------------------------------------------------------------------------- has
	 */
	b8 has(T* data)
	{
		if (!root)
			return false;
		return search(root, data).state == SearchResult::Found;
	}

	/* -------------------------------------------------------------------------------------------- insert
	 *  Attempts to insert the given data. If the data already exists then nothing happens.
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
		
		Node* search_node = root;
		b8 place_right = true;
		for (;;)
		{
			if (search_node->data == data)
				return;
			
			if (search_node->data > data)
			{
				if (search_node->left)
					search_node = search_node->left;
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

		// just renaming, we dont actually need to make new 
		// nodes pointers here but it makes the code easier to read.
		// though maybe if i need to return nodes later it will be
		// practically useful
		Node* parent = search_node;
		Node* child = new_node;

		// walk back up the tree and adjust balance
		for (;;)
		{
			if (parent->right == child)
			{
				
			}
		}
	}

	/* --------------------------------------------------------------------------------------------
	 *  Internal helpers
	 */
private:
	struct SearchResult
	{
		enum {
			Found,
			MissingLeft,
			MissingRight,
		} result;

		Node* node;
	};

	/* -------------------------------------------------------------------------------------------- search
	 *  Searches for a node representing the given data.
	 */
	SearchResult search(Node* node, T* data)
	{
		for (;;)
		{
			if (node->data == data)
				return {SearchResult::Found, node};

			b8 less = node->data > data;

			if (less)
			{
				if (node->left)
				{
					node = node->left;
					continue;
				}
			}
			else if (node->right)
			{
				node = node->right;
				continue;
			}
			
			return {less ? SearchResult::MissingLeft : SearchResult::MissingRight, node};
		}
	}


	/* -------------------------------------------------------------------------------------------- rotate_left
	 *  (A a (B b g))
	 *  ->
	 *  (B (A a b) g)
	 */
	Node* rotate_left(Node* A, Node* B)
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

	/* -------------------------------------------------------------------------------------------- rotate_right
	 *  (B (A a b) g)
	 *  ->
	 *  (A a (B b g))
	 */
	Node* rotate_right(Node* A, Node* B)
	{
		Node* b = A->right;
		B->left = b;
		if (b)
			b->parent = B;
		A->right = B;
		B->parent = A;

		if (!A->balance_factor)
		{
			B->balance_factor =  1;
			A->balance_factor = -1;
		}
		else
		{
			A->balance_factor = 
			B->balance_factor = 0;
		}
	}

	
	/* -------------------------------------------------------------------------------------------- rotate_right_left
	 *  (A a (C (B b c) d))
	 *  -> 
	 *  (A a (B b (C c d)))
	 *  ->
	 *  (B (A a b) (C c d))
	 */
	Node* rotate_right_left(Node* A, Node* C)
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
	}

	/* -------------------------------------------------------------------------------------------- rotate_right_left
	 *  (B (A a b) (C c d))
	 *  -> 
	 *  (A a (B b (C c d)))
	 *  ->
	 *  (A a (C (B b c) d))
	 */

};

#endif
