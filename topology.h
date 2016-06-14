#ifndef __LIBMAPPING_TOPOLOGY_H__
#define __LIBMAPPING_TOPOLOGY_H__

typedef struct topology_t {
	graph_t graph;
	vertex_t *root;
	uint32_t pu_number;
	uint32_t n_levels;
	uint32_t *arities;
	uint32_t *link_weights;
	uint32_t *best_pus;
	uint32_t *nobjs_per_level;
	uint32_t n_numa_nodes, n_pus_per_numa_node;
	uint32_t **pus_of_numa_node; // [n_numa_nodes][n_pus_per_numa_node]
	uint32_t *pus_to_numa_node;
	// pu_t *pus;
	struct topology_t *opt_topology; // doesn't have *pus

	uint32_t page_size;
	uint32_t page_shift;
	unsigned long page_addr_mask;
	unsigned long offset_addr_mask;

	uint32_t dist_pus_dim;
	uint32_t dist_pus_dim_log;
	uint32_t *dist_pus_;
} topology_t;

typedef struct machine_t {
	char name[256];
	topology_t topology;
} machine_t;

void libmapping_topology_init (void);
void libmapping_topology_destroy (void);

static inline uint32_t libmapping_topology_dist_pus (topology_t *t, uint32_t pu1, uint32_t pu2)
{
	return t->dist_pus_[ (pu1 << t->dist_pus_dim_log) + pu2 ];
}

typedef int (*libmapping_topology_walk_routine_t)(void*, vertex_t*, vertex_t*, edge_t*, uint32_t);
// prototype for walk:
// int func (void *data, vertex_t *current_vertex, vertex_t *previous_vertex, edge_t *edge, uint32_t level)
void libmapping_topology_walk_pre_order(topology_t *topology, libmapping_topology_walk_routine_t routine, void *data);
//void libmapping_topology_walk_pos_order(libmapping_topology_walk_routine_t routine, void *data);

void libmapping_topology_analysis (topology_t *t);

void libmapping_topology_print (topology_t *t);

vertex_t* libmapping_create_fake_topology (topology_t *topology, uint32_t *arities, uint32_t nlevels, uint32_t *pus, weight_t *weights);
void libmapping_get_n_pus_fake_topology (uint32_t *arities, uint32_t nlevels, uint32_t *npus, uint32_t *nvertices);

#endif
