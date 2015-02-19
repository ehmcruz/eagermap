#include "libmapping.h"

struct topology_walk_tmp_t {
	topology_t *t;
	uint32_t i, j;
};

topology_t libmapping_topology;

// too lazy to replace topology to libmapping_topology
#define topology libmapping_topology

#define dist(i, j) dist_[ ((i) << dist_dim_log) + (j) ]
#define dist_pus(i, j) t->dist_pus_[ ((i) << t->dist_pus_dim_log) + (j) ]

static void floyd_warshall (topology_t *t)
{
	uint32_t i, j, k;
	uint32_t *dist_;
	uint32_t dist_dim;
	uint32_t dist_dim_log;

	dist_dim = libmapping_get_next_power_of_two(t->graph.n_vertices);
	dist_dim_log = libmapping_get_log2(dist_dim);
	t->dist_pus_dim = libmapping_get_next_power_of_two(t->pu_number);
	t->dist_pus_dim_log = libmapping_get_log2(t->dist_pus_dim);

	dist_ = (uint32_t*)lm_calloc(dist_dim*dist_dim, sizeof(uint32_t));
	t->dist_pus_ = (uint32_t*)lm_calloc(t->dist_pus_dim*t->dist_pus_dim, sizeof(uint32_t));

	for (i=0; i<t->graph.n_vertices; i++) {
		dist(i, i) = 0;
		for (j=i+1; j<t->graph.n_vertices; j++) {
			dist(i, j) = dist(j, i) = USHRT_MAX; //UINT_MAX;
		}
	}

	for (i=0; i<t->graph.n_vertices; i++) {
		for (j=0; j<t->graph.vertices[i].arity; j++) {
			dist(i, t->graph.vertices[i].linked[j].v->pos) = t->graph.vertices[i].linked[j].e->weight;
		}
	}

	for (k=0; k<t->graph.n_vertices; k++) {
		for (i=0; i<t->graph.n_vertices; i++) {
			for (j=0; j<t->graph.n_vertices; j++) {
				uint32_t sum;
				sum = dist(i, k) + dist(k, j);
				//printf("sum is %llu\n", sum);
				if (dist(i, j) > sum) {
					dist(i, j) = sum;
				}
			}
		}
	}

	for (i=0; i<t->pu_number; i++) {
		for (j=0; j<t->pu_number; j++) {
			dist_pus(i, j) = 0;
		}
	}

	for (i=0; i<t->graph.n_vertices; i++) {
		for (j=0; j<t->graph.n_vertices; j++) {
			if (t->graph.vertices[i].type == GRAPH_ELTYPE_PU && t->graph.vertices[j].type == GRAPH_ELTYPE_PU) {
				dist_pus(t->graph.vertices[i].id, t->graph.vertices[j].id) = dist(i, j);
			}
		}
	}

	lm_free(dist_);
}

static int topology_walk_pre_order(libmapping_topology_walk_routine_t routine, vertex_t *v, vertex_t *from, edge_t *edge, void *data, uint32_t level)
{
	if (routine(data, v, from, edge, level)) {
		uint32_t i;

		for (i=0; i<v->arity; i++) {
			if (v->linked[i].v != from) {
				if (!topology_walk_pre_order(routine, v->linked[i].v, v, v->linked[i].e, data, level + 1))
					return 0;
			}
		}

		return 1;
	}
	else
		return 0;
}

void libmapping_topology_walk_pre_order (topology_t *topology, libmapping_topology_walk_routine_t routine, void *data)
{
	topology_walk_pre_order(routine, topology->root, NULL, NULL, data, 0);
}

topology_t* libmapping_topology_get ()
{
	return &topology;
}


void libmapping_get_n_pus_fake_topology (uint32_t *arities, uint32_t nlevels, uint32_t *npus, uint32_t *nvertices)
{
	int j;
	if (nlevels == 0) {
		(*npus)++;
		(*nvertices)++;
	}
	else {
		(*nvertices)++;
		for (j=0; j < *arities; j++) {
			libmapping_get_n_pus_fake_topology(arities+1, nlevels-1, npus, nvertices);
		}
	}
}

static vertex_t* create_fake_topology (uint32_t level, uint32_t *arities, uint32_t nlevels, uint32_t *pus, weight_t *weights, uint32_t numa_node)
{
	int j;
	vertex_t *v, *link;
	edge_t *e;

	v = libmapping_get_free_vertex(&topology.graph);

	if (nlevels == 0) {
		static uint32_t id = 0;
		v->type = GRAPH_ELTYPE_PU;
		if (pus != NULL)
			v->id = pus[id];
		else
			v->id = id;
		id++;
	}
	else {
		if (numa_node && level == 1) {
			static uint32_t id = 0;
			v->type = GRAPH_ELTYPE_NUMA_NODE;
			v->id = id++;
		}
		else {
			v->type = GRAPH_ELTYPE_UNDEFINED;
			v->id = 0;
		}
		for (j=0; j < *arities; j++) {
			link = create_fake_topology(level+1, arities+1, nlevels-1, pus, (weights == NULL) ? NULL : weights+1, numa_node);
			e = libmapping_graph_connect_vertices(&topology.graph, v, link);
			if (weights == NULL)
				e->weight = 1 << (nlevels);
			else
				e->weight = *weights;
			e->type = GRAPH_ELTYPE_UNDEFINED;
		}
	}

	return v;
}

vertex_t* libmapping_create_fake_topology(uint32_t *arities, uint32_t nlevels, uint32_t *pus, weight_t *weights)
{
	return create_fake_topology(0, arities, nlevels, pus, weights, 0);
}

static int print_topology(void *d, vertex_t *v, vertex_t *from, edge_t *edge, uint32_t level)
{
	uint32_t i;

	for (i=0; i < level; i++)
		lm_printf("\t");
	lm_printf("%s (id: %u, arity: %u)\n", libmapping_graph_eltype_str(v->type), v->id, (v->type == GRAPH_ELTYPE_ROOT) ? v->arity : v->arity - 1);

	return 1;
}


void libmapping_topology_print(topology_t *t)
{
	libmapping_topology_walk_pre_order(t, print_topology, NULL);
}


static vertex_t* optimize_topology_(vertex_t *obj, vertex_t *from, weight_t ac_weight, topology_t *opt, uint32_t level)
{
	uint32_t i;
	vertex_t *v, *link;
	edge_t *e;

	if (obj->type != GRAPH_ELTYPE_ROOT && obj->type != GRAPH_ELTYPE_PU) {
		if (obj->arity == 2) {
			if (obj->linked[0].v != from)
				return optimize_topology_(obj->linked[0].v, obj, ac_weight+obj->linked[0].e->weight, opt, level+1);
			else
				return optimize_topology_(obj->linked[1].v, obj, ac_weight+obj->linked[1].e->weight, opt, level+1);
		}
	}

	v = libmapping_get_free_vertex(&opt->graph);

	v->weight = 0;
	v->id = obj->id;
	v->type = obj->type;

	for (i=0; i<obj->arity; i++) {
		if (obj->linked[i].v != from) {
			link = optimize_topology_(obj->linked[i].v, obj, 0, opt, level+1);
			e = libmapping_graph_connect_vertices(&opt->graph, v, link);
			e->weight = ac_weight + obj->linked[i].e->weight;
			e->type = GRAPH_ELTYPE_UNDEFINED;
		}
	}

	return v;
}

static void optimize_topology (topology_t *t, topology_t *opt)
{
	libmapping_graph_init(&opt->graph, t->graph.n_vertices, t->graph.n_vertices - 1);
	opt->root = optimize_topology_(t->root, NULL, 0, opt, 0);
}

static int get_topology_attr_pu_number (void *d, vertex_t *v, vertex_t *from, edge_t *edge, uint32_t level)
{
	topology_t *t = (topology_t*)d;
	if (v->type == GRAPH_ELTYPE_PU)
		t->pu_number++;
	return 1;
}

static int detect_n_levels (void *d, vertex_t *v, vertex_t *previous_vertex, edge_t *edge, uint32_t level)
{
	topology_t *t = (topology_t*)d;
	t->n_levels++;
	if (v->type == GRAPH_ELTYPE_PU)
		return 0;
	else
		return 1;
}

static int get_topology_best_pus_number (void *d, vertex_t *v, vertex_t *from, edge_t *edge, uint32_t level)
{
	struct topology_walk_tmp_t *stc = (struct topology_walk_tmp_t*)d;
	if (v->type == GRAPH_ELTYPE_PU) {
		stc->t->best_pus[stc->i] = v->id;
		stc->i++;
	}
	return 1;
}

static int detect_arity_of_levels (void *d, vertex_t *v, vertex_t *previous_vertex, edge_t *edge, uint32_t level)
{
	struct topology_walk_tmp_t *stc = (struct topology_walk_tmp_t*)d;

	if (v->type == GRAPH_ELTYPE_ROOT) {
		stc->t->arities[stc->i] = v->arity;
	}
	else {
		stc->t->arities[stc->i] = v->arity - 1;
	}

	stc->i++;

	if (v->type == GRAPH_ELTYPE_PU)
		return 0;
	else
		return 1;
}

static int detect_link_weight_of_levels (void *d, vertex_t *v, vertex_t *previous_vertex, edge_t *edge, uint32_t level)
{
	struct topology_walk_tmp_t *stc = (struct topology_walk_tmp_t*)d;

	if (v->type == GRAPH_ELTYPE_ROOT)
		return 1;

	stc->t->link_weights[stc->i] = edge->weight;
	stc->i++;

	if (v->type == GRAPH_ELTYPE_PU)
		return 0;
	else
		return 1;
}

static int get_n_numa_nodes (void *d, vertex_t *v, vertex_t *from, edge_t *edge, uint32_t level)
{
	struct topology_walk_tmp_t *stc = (struct topology_walk_tmp_t*)d;
	if (v->type == GRAPH_ELTYPE_NUMA_NODE) {
		stc->t->n_numa_nodes++;
	}
	return 1;
}

static int get_pus_of_numa_nodes (void *d, vertex_t *v, vertex_t *from, edge_t *edge, uint32_t level)
{
	struct topology_walk_tmp_t *stc = (struct topology_walk_tmp_t*)d;
	if (v->type == GRAPH_ELTYPE_NUMA_NODE) {
		stc->i = v->id;
		stc->j = 0;
	}
	else if (v->type == GRAPH_ELTYPE_PU) {
		stc->t->pus_of_numa_node[stc->i][stc->j] = v->id;
		stc->j++;
	}
	return 1;
}

static void topology_analysis_ (topology_t *t, topology_t *orig)
{
	struct topology_walk_tmp_t tmp;
	int32_t i, j;

	tmp.t = t;

	t->pu_number = 0;
	libmapping_topology_walk_pre_order(t, get_topology_attr_pu_number, t);

	t->n_levels = 0;
	libmapping_topology_walk_pre_order(t, detect_n_levels, t);

	if (orig == NULL) {
		t->best_pus = (uint32_t*)lm_calloc(t->pu_number, sizeof(uint32_t));

		tmp.i = 0;
		libmapping_topology_walk_pre_order(t, get_topology_best_pus_number, &tmp);
	}
	else {
		t->best_pus = orig->best_pus;
	}

	if (orig == NULL) {
		t->page_size = 1 << RubyConfig::pageSizeBits();
		t->page_shift = libmapping_get_log2(t->page_size);
		t->offset_addr_mask = t->page_size - 1;
		t->page_addr_mask = ~t->offset_addr_mask;
	}
	else {
		t->page_size = orig->page_size;
		t->page_shift = orig->page_shift;
		t->offset_addr_mask = orig->offset_addr_mask;
		t->page_addr_mask = orig->page_addr_mask;
	}

	t->arities = (uint32_t*)lm_calloc(t->n_levels, sizeof(uint32_t));

	t->link_weights = (uint32_t*)lm_calloc(t->n_levels, sizeof(uint32_t));

	t->nobjs_per_level = (uint32_t*)lm_calloc(t->n_levels, sizeof(uint32_t));

	tmp.i = 0;
	libmapping_topology_walk_pre_order(t, detect_arity_of_levels, &tmp);

	tmp.i = 0;
	libmapping_topology_walk_pre_order(t, detect_link_weight_of_levels, &tmp);

	t->nobjs_per_level[t->n_levels-1] = t->pu_number;
	for (i=t->n_levels-2; i>=0; i--)
		t->nobjs_per_level[i] = t->nobjs_per_level[i+1] / t->arities[i];

	if (orig == NULL) {
		t->n_numa_nodes = 0;
		libmapping_topology_walk_pre_order(t, get_n_numa_nodes, &tmp);

		if (t->n_numa_nodes > 0) {
			t->n_pus_per_numa_node = t->pu_number / t->n_numa_nodes;
			t->pus_of_numa_node = (uint32_t**)libmapping_matrix_malloc(t->n_numa_nodes, t->n_pus_per_numa_node, sizeof(uint32_t));
			libmapping_topology_walk_pre_order(t, get_pus_of_numa_nodes, &tmp);
		}
		else {
			t->n_numa_nodes = 1;
			t->n_pus_per_numa_node = t->pu_number;
			t->pus_of_numa_node = (uint32_t**)libmapping_matrix_malloc(t->n_numa_nodes, t->n_pus_per_numa_node, sizeof(uint32_t));

			for (i=0; i<t->pu_number; i++)
				t->pus_of_numa_node[0][i] = t->best_pus[i];
		}

		t->pus_to_numa_node = (uint32_t*)lm_calloc(t->pu_number, sizeof(uint32_t));

		for (i=0; i<t->pu_number; i++)
			t->pus_to_numa_node[i] = 0xFFFFFFFF;

		for (i=0; i<t->n_numa_nodes; i++) {
			for (j=0; j<t->n_pus_per_numa_node; j++)
				t->pus_to_numa_node[ t->pus_of_numa_node[i][j] ] = i;
		}

	}
	else {
		t->n_numa_nodes = orig->n_numa_nodes;
		t->n_pus_per_numa_node = orig->n_pus_per_numa_node;
		t->pus_of_numa_node = orig->pus_of_numa_node;
		t->pus_to_numa_node = orig->pus_to_numa_node;
	}

	floyd_warshall(t);
}

void libmapping_topology_analysis (topology_t *t)
{
	t->opt_topology = (topology_t*)lm_calloc(1, sizeof(topology_t));
	t->opt_topology->opt_topology = t->opt_topology;

	topology_analysis_(t, NULL);
	return; // %%% optimize_topology crashes:

	optimize_topology(t, t->opt_topology);

	topology_analysis_(t->opt_topology, t);
}
