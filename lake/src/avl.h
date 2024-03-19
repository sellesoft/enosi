/*
 *
 *  AVL tree implementation
 *	
 *	The tree is specialized to work with unique memory addresses and so does not 
 *	deal with hashing its elements and sorts by the element's address. It will
 *	primarily be used as a set.
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
		// TODO(sushi) we may be able to do something neat where we instead store offsets from the nodes position in the 
		//             pool with s32s or even like s16 to save some space so experiment later
		//             this would require the pools chunks being connected via doubly connected list
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
		return false;
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

		new_node->data = data;

		// walk back up the tree and adjust balance
		for (Node* parent = search_node,* child = new_node; parent; child = parent, parent = child->parent)
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
						new_child = rotate_right_left(parent, child);
					else 
						new_child = rotate_left(parent, child);
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
						new_child = rotate_left_right(parent, child);
					else
						new_child = rotate_right(parent, child);
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
	}

	dstr to_string()
	{
		dstr out = dstr::create();
	
		if (!root)
		{
			out.append("X\n");
			return out;
		}
			
		to_string_recursive(out, root, 0);

		out.append("\n");

		return out;
	}

	/* --------------------------------------------------------------------------------------------
	 *  Internal helpers
	 */
private:

	void to_string_recursive(dstr& out, Node* n, u32 layers)
	{
		for (s32 i = 0; i < layers; i++) out.append("  ");

		if (n->right || n->left)
			out.append("(");
		else
		{
			out.append((s64)n->data);
			return;
		}

		out.appendv((s64)n->data, " ", n->balance_factor, "\n");

		if (n->left)
			to_string_recursive(out, n->left, layers + 1);
		else
		{
			for (s32 i = 0; i < layers + 1; i++) out.append("  ");
			out.append("X");
		}

		out.append("\n");
		if (n->right)
			to_string_recursive(out, n->right, layers + 1);
		else
		{
			for (s32 i = 0; i < layers + 1; i++) out.append("  ");
			out.append("X");
		}


		if (n->right || n->left)
			out.append(")");
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

		return A;
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

		return B;
	}

	/* -------------------------------------------------------------------------------------------- rotate_left_right
	 *  (A (C a (B b c)) d)
	 *  ->
	 *  (A (B (C a b) c) d)
	 *  ->
	 *  (B (C a b) (A c d))
	 */
	Node* rotate_left_right(Node* A, Node* C)
	{
		Node* B = C->right;
		Node* b = C->left;
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
};

#endif
