#ifndef __LIBMAPPING_GRAPH_H__
#define __LIBMAPPING_GRAPH_H__

typedef int64_t weight_t;

#define __GRAPH_ELTYPE__(TYPE) GRAPH_ELTYPE_##TYPE,
typedef enum graph_eltype_t {
	#include "graph-eltypes.h"
	GRAPH_ELTYPE_NUMBER
} graph_eltype_t;
#undef __GRAPH_ELTYPE__

struct edge_t;
struct vertex_t;

typedef struct vertex_edge_t {
	struct edge_t *e;
	struct vertex_t *v;
} vertex_edge_t;

typedef struct vertex_t {
	graph_eltype_t type;
	weight_t weight;
	uint32_t id; // id used by topology
	
	uint32_t pos; // pos in graph.vertices
	
	vertex_edge_t *linked;
	uint32_t arity;
} vertex_t;

typedef struct edge_t {
	graph_eltype_t type;
	weight_t weight;
	uint32_t id;
	
	vertex_t *src;
	vertex_t *dest;
} edge_t;

typedef struct graph_t {
	vertex_t *vertices;
	edge_t *edges;
	
	uint32_t n_vertices;
	uint32_t n_edges;
	
	// internal use only
	vertex_t *next_vertex;
	edge_t *next_edge;
	uint32_t used_vertices;
	uint32_t used_edges;
} graph_t;

void libmapping_graph_init (graph_t *g, uint32_t n_vertices, uint32_t n_edges);
void libmapping_graph_destroy (graph_t *g);
vertex_t* libmapping_get_free_vertex(graph_t *g);
edge_t* libmapping_get_free_edge(graph_t *g);
const char* libmapping_graph_eltype_str(graph_eltype_t type);
edge_t* libmapping_graph_connect_vertices(graph_t *g, vertex_t *src, vertex_t *dest);

#endif
