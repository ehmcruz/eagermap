#include "libmapping.h"

#include <omp.h>
#include <assert.h>

typedef enum group_type_t {
	GROUP_TYPE_THREAD,
	GROUP_TYPE_GROUP
} group_type_t;

typedef struct thread_group_t {
	group_type_t type;
	uint32_t id;
	uint32_t nelements;
	struct thread_group_t **elements;
} thread_group_t;

typedef struct my_data_t {
	uint32_t arities[16];
	uint32_t exec_el_in_level[16];
	uint32_t levels_n;
	thread_group_t **groups; // one for each level
	thread_group_t root_group;
	comm_matrix_t matrix_[2];
	topology_t *hardware_topology;
	uint32_t winners[MAX_THREADS];
	uint32_t chosen[MAX_THREADS];
} my_data_t;

#define PADDING_SIZE 128
#define MAX_PARALLEL_THREADS 1024

typedef struct greedy_parallel_t {
	int32_t local_winner;
	weight_t local_wmax;
} __attribute__ ((aligned (PADDING_SIZE))) greedy_parallel_t;

static greedy_parallel_t pcontext[MAX_PARALLEL_THREADS] __attribute__((aligned(PADDING_SIZE)));
static int32_t pthreshold = 500;
static uint8_t parallel_enabled = 1;

static void generate_group(my_data_t *this, comm_matrix_t *m, uint32_t total_elements, uint32_t group_elements, thread_group_t *group, uint32_t level, uint32_t *chosen)
{
	weight_t w, wmax;
	uint32_t winner = 0;
	uint32_t i, j, k;

	for (i=0; i<group_elements; i++) { // in each iteration, find one element of the group
		wmax = -1;
		for (j=0; j<total_elements; j++) { // iterate over all elements to find the one that maximizes the communication relative to the elements that are already in the group
			if (!chosen[j]) {
				w = 0;
				for (k=0; k<i; k++) {
					w += comm_matrix_ptr_el(m, j, this->winners[k]);
				}
				if (w > wmax) {
					wmax = w;
					winner = j;
				}
			}
		}

		chosen[winner] = 1;
		this->winners[i] = winner;
		group->elements[i] = &this->groups[level-1][winner];
	}
}

static void generate_group_openmp(my_data_t *this, comm_matrix_t *m, uint32_t total_elements, uint32_t group_elements, thread_group_t *group, uint32_t level, uint32_t *chosen)
{
	weight_t w, wmax;
	int32_t winner;
	uint32_t i, j, k, my_id, pnt;
	
/*	dprintf2("Greedy going parallel total_elements %u group_elements %u\n", total_elements, group_elements);*/
	
	for (i=0; i<group_elements; i++) { // in each iteration, I will find one element of the group
/*		libmapping_print_matrix(m, stdout);*/
		
		#pragma omp parallel default(none) private(j, w, k, my_id) shared(pnt, total_elements, chosen, this, m, i, pcontext)
		{
			#pragma omp master
			{
				pnt = omp_get_num_threads();
				assert(pnt <= MAX_PARALLEL_THREADS);
/*				printf("pnt %i\n", pnt);*/
			}
			
			my_id = omp_get_thread_num();
			assert(my_id < MAX_PARALLEL_THREADS);
			
			pcontext[my_id].local_wmax = -1;

			#pragma omp for schedule(guided)
			for (j=0; j<total_elements; j++) { // I will iterate over all elements to find the one that maximizes the communication relative to the elements that are already in the group
				if (!chosen[j]) {
					w = 0;
					for (k=0; k<i; k++) {
						w += comm_matrix_ptr_el(m, j, this->winners[k]);
	/*					lm_printf("   m[%u][%u] = %llu", j, winners[k], m->values[j][ winners[k] ]);*/
					}
	/*				lm_printf("\n");*/
	/*				if (i > 0)*/
	/*					dprintf2("   max[%u] = %llu\n", j, w);*/
					if (w > pcontext[my_id].local_wmax) {
						pcontext[my_id].local_wmax = w;
						pcontext[my_id].local_winner = j;
					}
				}
			}
		}
		
		wmax = -1;
		winner = -1;
		for (j=0; j<pnt; j++) {
			if (pcontext[j].local_wmax > wmax) {
				wmax = pcontext[j].local_wmax;
				winner = pcontext[j].local_winner;
			}
		}
		
		LM_ASSERT(winner != -1)
		
/*		dprintf2("   winner[%u] is %u (wmax "PRINTF_UINT64")\n", i, winner, wmax);//getchar();*/
		chosen[winner] = 1;
		this->winners[i] = winner;
		group->elements[i] = &this->groups[level-1][winner];
	}
}

static uint32_t generate_groups(my_data_t *this, comm_matrix_t *m, uint32_t nelements, uint32_t level)
{
	uint32_t el_per_group, done, leftover, ngroups, avl_groups, group_i, in_group, i;
	thread_group_t *group;

/*printf("ggg nelements %i level %i levels_n %i\n", nelements, level, levels_n);*/

	avl_groups = this->exec_el_in_level[level];

	ngroups = (nelements > avl_groups) ? avl_groups : nelements;

	el_per_group = nelements / ngroups;
	leftover = nelements % ngroups;

	group_i = 0;

	for (i=0; i<nelements; i++)
		this->chosen[i] = 0;

/*printf("hhh nelements %i level %i levels_n %i\n", nelements, level, levels_n);*/

	for (done=0; done<nelements; done+=in_group) {
		in_group = el_per_group;
		if (leftover) {
			in_group++;
			leftover--;
		}

		assert(group_i < this->exec_el_in_level[level]);

		group = &this->groups[level][group_i];
		group->type = GROUP_TYPE_GROUP;
		group->nelements = in_group;
		group->id = group_i;
		
		assert(in_group <= this->exec_el_in_level[level-1]);

/*printf("kkk nelements %i done %i group->nelements %i level %i group_i %i levels_n %i\n", nelements, done, group->nelements, level, group_i, levels_n);*/

		if (parallel_enabled && nelements >= pthreshold) {
/*			printf("going parallel nelements %i\n", nelements);*/
			generate_group_openmp(this, m, nelements, in_group, group, level, this->chosen);
		}
		else
			generate_group(this, m, nelements, in_group, group, level, this->chosen);

/*printf("jjj nelements %i done %i group->nelements %i level %i group_i %i levels_n %i\n", nelements, done, group->nelements, level, group_i, levels_n);*/

		group_i++;
	}

	return ngroups;
}

static void map_groups_to_topology_ (my_data_t *this, vertex_t *v, vertex_t *from, thread_group_t *g, uint32_t *map, uint32_t level)
{
	uint32_t i;

/*printf("v->type %i %s v->arity %i g->nelements %i\n", v->type, libmapping_graph_eltype_str(v->type), v->arity, g->nelements);*/

	if (v->type == GRAPH_ELTYPE_PU) {

		for (i=0; i<g->nelements; i++) {
			map[ g->elements[i]->id ] = v->id;
		}
	}
	else if ((v->type == GRAPH_ELTYPE_ROOT && v->arity >= 2) || v->arity > 2) { // is shared level
		for (i=0; i<g->nelements; i++) {
			if (v->linked[i].v != from) {
/*printf("here %i\n", i);*/
				map_groups_to_topology_(this, v->linked[i].v, v, g->elements[i], map, level+1);
			}
		}
	}
	else {
		if (v->type == GRAPH_ELTYPE_ROOT) { // non-shared root, arity is 1
			map_groups_to_topology_(this, v->linked[0].v, v, g, map, level+1);
		}
		else { // non-shared intermediate node, arity is 2
			if (v->linked[0].v != from)
				map_groups_to_topology_(this, v->linked[0].v, v, g, map, level+1);
			else
				map_groups_to_topology_(this, v->linked[1].v, v, g, map, level+1);
		}
	}
}

static void map_groups_to_topology (my_data_t *this, topology_t *t, thread_group_t *g, uint32_t *map)
{
	map_groups_to_topology_(this, t->root, NULL, g, map, 0);
}

static void recreate_matrix (my_data_t *this, comm_matrix_t *old, thread_group_t *group_set, uint32_t ngroups, comm_matrix_t *m)
{
	uint32_t i, j, k, z;
	weight_t w;

	m->nthreads = ngroups;

	for (i=0; i<ngroups-1; i++) {
		for (j=i+1; j<ngroups; j++) {
			w = 0;
			for (k=0; k<group_set[i].nelements; k++) {
				for (z=0; z<group_set[j].nelements; z++) {
					w += comm_matrix_ptr_el(old, group_set[i].elements[k]->id, group_set[j].elements[z]->id );
				}
			}

			comm_matrix_ptr_write(m, i, j, w);
		}
	}
}

struct params_t {
	my_data_t *this;
	uint32_t pos;
};

static int detect_levels_with_sharers(void *data, vertex_t *v, vertex_t *previous_vertex, edge_t *edge, uint32_t level)
{
	struct params_t *params = (struct params_t*)data;
	my_data_t *this = params->this;

	if ((v->type == GRAPH_ELTYPE_ROOT && v->arity >= 2) || v->arity > 2) // is shared level
		this->levels_n++;
	if (v->type == GRAPH_ELTYPE_PU)
		return 0;
	else
		return 1;
}

static int detect_arity_of_levels_with_sharers(void *data, vertex_t *v, vertex_t *previous_vertex, edge_t *edge, uint32_t level)
{
	struct params_t *params = (struct params_t*)data;
	uint32_t *pos = &params->pos;
	my_data_t *this = params->this;

	if (v->type == GRAPH_ELTYPE_ROOT && v->arity >= 2) { // is shared level
		this->arities[*pos] = v->arity;
		(*pos)--;
	}
	else if (v->arity > 2) { // is shared level
		this->arities[*pos] = v->arity - 1;
		(*pos)--;
	}
	if (v->type == GRAPH_ELTYPE_PU)
		return 0;
	else
		return 1;
}

static void alloc_group_tree (my_data_t *this, thread_map_alg_init_t *data)
{
	uint32_t i, j;
	
	this->groups = lm_calloc(this->levels_n, sizeof(thread_group_t*));
	assert(this->groups != NULL);
	
	for (i=0; i<this->levels_n; i++) {
		this->groups[i] = lm_calloc(this->exec_el_in_level[i], sizeof(thread_group_t));
		assert(this->groups[i] != NULL);
	}
	
	for (j=0; j<this->exec_el_in_level[0]; j++)
		this->groups[0][j].elements = NULL;
	
	for (i=1; i<this->levels_n; i++) {
		for (j=0; j<this->exec_el_in_level[i]; j++) {
			this->groups[i][j].elements = lm_calloc(this->exec_el_in_level[i-1], sizeof(thread_group_t*));
			assert(this->groups[i][j].elements != NULL);
		}
	}
	
	this->root_group.elements = lm_calloc(this->exec_el_in_level[this->levels_n-1], sizeof(thread_group_t*));
	assert(this->root_group.elements != NULL);
}

void* libmapping_mapping_algorithm_greedy_init (thread_map_alg_init_t *data)
{
	uint32_t i, pnt;
	my_data_t *this;
	struct params_t params;
	
	this = malloc(sizeof(my_data_t));
	assert(this != NULL);

	this->hardware_topology = data->topology;
	this->levels_n = 2;
	params.this = this;
	libmapping_topology_walk_pre_order(data->topology, detect_levels_with_sharers, &params);

	this->arities[0] = this->arities[1] = 1; // first arities should always be 1
	params.pos = this->levels_n - 1;
	libmapping_topology_walk_pre_order(data->topology, detect_arity_of_levels_with_sharers, &params);

	this->exec_el_in_level[0] = data->nt;
	this->exec_el_in_level[1] = data->topology->pu_number;
	for (i=2; i<this->levels_n; i++) {
		this->exec_el_in_level[i] = this->exec_el_in_level[i - 1] / this->arities[i];
	}

	for (i=this->levels_n-1; i>0; i--) {
		if (this->exec_el_in_level[i] == 1) {
			this->levels_n = i;
		}
	}

	libmapping_comm_matrix_init(&this->matrix_[0], data->nt);
	libmapping_comm_matrix_init(&this->matrix_[1], data->nt);
	
/*	printf("used %u levels\n", levels_n);*/
	alloc_group_tree(this, data);
	
	libmapping_env_get_integer("EAGERMAP_PARALLEL", &pthreshold);
	printf("setting eagermap parallel threshold to %i\n", pthreshold);

	#pragma omp parallel default(none) shared(pnt)
	{
		#pragma omp master
		{
			pnt = omp_get_num_threads();
		}
	}
	
	printf("number of threads: %u\n", pnt);

	return this;
}

void libmapping_mapping_algorithm_greedy_map (thread_map_alg_map_t *data)
{
	uint32_t nelements, i, ngroups, matrix_i;
	comm_matrix_t *m, *oldm;
	my_data_t *this;
	
	this = (my_data_t*)data->init_data;

	matrix_i = 0;

/*printf("blah 2\n");*/
/*for (i=0; i<MAX_THREADS; i++) printf("%i ", data->map[i]);*/
/*printf("\n");*/
/*printf("data->m_init->nthreads %i\n", data->m_init->nthreads);*/

	for (i=0; i<data->m_init->nthreads; i++) {
		this->groups[0][i].type = GROUP_TYPE_THREAD;
		this->groups[0][i].id = i;
		this->groups[0][i].nelements = 0;
	}

	nelements = data->m_init->nthreads;
	m = data->m_init;

	for (i=1; i<this->levels_n; i++) {
		ngroups = generate_groups(this, m, nelements, i);

		oldm = m;
		m = &this->matrix_[matrix_i];
		matrix_i ^= 1; // switch buffers on every iteration

		if (i < (this->levels_n-1)) {

			recreate_matrix(this, oldm, this->groups[i], ngroups, m);
		}

		nelements = ngroups;
	}

	this->root_group.type = GROUP_TYPE_GROUP;
	this->root_group.nelements = nelements;
	
/*printf("nelements %i exec_el_in_level[levels_n-1] %i\n", nelements, exec_el_in_level[levels_n-1]);*/
	assert(nelements <= this->exec_el_in_level[this->levels_n-1]);

	for (i=0; i<nelements; i++) {
		this->root_group.elements[i] = &this->groups[this->levels_n-1][i];
	}

	map_groups_to_topology(this, this->hardware_topology, &this->root_group, data->map);
}

