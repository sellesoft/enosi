#include "lake.h"
#include "stdlib.h"

int main(const int argc, const char* argv[]) 
{
	lake.init(strl("lakefile"), argc, argv);
	lake.run();
}

//void visit(Graph::Vertex* v, SList<Thing>* sorted)
//{
//	auto t = (Thing*)v->data;
//
//	printv("visiting ", t->c, "\n");
//
//	if (t->perm_mark)
//		return;
//
//	if (t->temp_mark)
//		error_nopath("cycle detected");
//
//	t->temp_mark = true;
//
//	for (Graph::VertexList::Node* iter = v->neighbors.head; iter; iter = iter->next)
//		visit(iter->data, sorted);
//
//	t->temp_mark = false;
//	t->perm_mark = true;
//
//	printv("pushing ", t->c, "\n");
//
//	sorted->push(t);
//}
//
//void print_graph(Graph* g) 
//{
//	auto sorted = SList<Thing>::create();
//
//	for (Graph::VertexList::Node* iter = g->vertexes.head; iter; iter = iter->next)
//	{
//		if (!((Thing*)iter->data->data)->perm_mark)
//			visit(iter->data, &sorted);
//	}
//
//	for (SList<Thing>::Node* n = sorted.head; n; n = n->next)
//	{
//		printv(n->data->c, "\n");
//	}
//}
