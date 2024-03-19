/* TODO(sushi) better name later
 * 
 *  Definition of the build systems tree structure and functionality.
 *
 */

#ifndef _lake_tree_h
#define _lake_tree_h

#include "common.h"

struct TreeNode;

struct TreeNodeList
{
	TreeNode** 
};

/* ------------------------------------------------------------------------------------------------
 */
struct TreeNode 
{
	TreeNode* first_prerequisite;
	TreeNode* last_prerequisite;
	
};

/* ------------------------------------------------------------------------------------------------
 */
struct Target
{
	str path;

};

#endif
