#include "libmapping.h"

#define __GRAPH_ELTYPE__(TYPE) #TYPE,
static const char *typenames[] = {
	#include "graph-eltypes.h"
	NULL
};

const char* libmapping_graph_eltype_str(graph_eltype_t type)
{
	return typenames[type];
}

void libmapping_graph_init (graph_t *g, uint32_t n_vertices, uint32_t n_edges)
{
	uint32_t i;

	g->vertices = (vertex_t*)lm_calloc(n_vertices, sizeof(vertex_t));

	g->edges = (edge_t*)lm_calloc(n_edges, sizeof(edge_t));

	g->vertices[0].linked = (vertex_edge_t*)lm_calloc(n_vertices * n_vertices, sizeof(vertex_edge_t));

	for (i=1; i<n_vertices; i++) {
		g->vertices[i].linked = g->vertices[0].linked + i*n_vertices;
	}

	for (i=0; i<n_vertices; i++)
		g->vertices[i].pos = i;

	g->n_vertices = n_vertices;
	g->n_edges = n_edges;

	g->next_vertex = g->vertices;
	g->next_edge = g->edges;
	g->used_vertices = 0;
	g->used_edges = 0;
}

void libmapping_graph_destroy (graph_t *g)
{
	lm_free(g->vertices[0].linked);
	lm_free(g->vertices);
	lm_free(g->edges);
}

vertex_t* libmapping_get_free_vertex(graph_t *g)
{
	g->used_vertices++;
	g->next_vertex->arity = 0;
	return g->next_vertex++;
}

edge_t* libmapping_get_free_edge(graph_t *g)
{
	g->used_edges++;
	return g->next_edge++;
}

edge_t* libmapping_graph_connect_vertices(graph_t *g, vertex_t *src, vertex_t *dest)
{
	edge_t *e;

	e = libmapping_get_free_edge(g);

	e->src = src;
	e->dest = dest;

	src->linked[src->arity].e = e;
	src->linked[src->arity].v = dest;

	dest->linked[dest->arity].e = e;
	dest->linked[dest->arity].v = src;

	src->arity++;
	dest->arity++;

	return e;
}
