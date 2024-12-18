/*
 *
 *  Graph structure and functionality.
 *  
 *  Later on this outta be specialized to how the graph actually works 
 *  so that we aren't using templating.
 */

#ifndef _iro_graph_h
#define _iro_graph_h

#include "common.h"
#include "pool.h"
#include "list.h"

namespace iro
{

/* ================================================================================================ Graph
 */
template<typename T>
struct Graph 
{
  struct Vertex;

  typedef SList<Vertex> VertexList;

  struct Vertex
  {
    T*         data;
    VertexList neighbors;
  };

  Pool<Vertex> pool;
  VertexList vertexes;
  s32 n_vertexes;


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */


  /* -------------------------------------------------------------------------------------------- create
   */
  static Graph<T> create()
  {
    Graph<T> out = {};
    out.pool = Pool<Vertex>::create();
    out.vertexes = SList<Vertex>::create();
    return out;
  }

  /* -------------------------------------------------------------------------------------------- destroy
   */
  void destroy()
  {
    pool.destroy();
    vertexes.destroy();
    n_vertexes = 0;
  }

  /* -------------------------------------------------------------------------------------------- addVertex
   *  Add a vertex to the graph that points to the given data, which may be null.
   *  This does not check if a vertex pointing to 'data' already exists!
   */
  Vertex* addVertex(T* data)
  {
    Vertex* v = pool.add();

    v->data = data;
    v->neighbors = VertexList::create();

    vertexes.push(v);
    n_vertexes += 1;

    return v;
  }

  /* -------------------------------------------------------------------------------------------- addEdge
   *  Create a directed edge from 'from' to 'to'. This does not check if the edge already 
   *  exists.
   */
  void addEdge(Vertex* from, Vertex* to)
  {
    from->neighbors.push(to);
  }
};

}

#endif
