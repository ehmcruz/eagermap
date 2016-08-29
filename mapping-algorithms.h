#ifndef __LIBMAPPING_MAPPING_ALGORITHMS_H__
#define __LIBMAPPING_MAPPING_ALGORITHMS_H__

typedef struct thread_map_alg_init_t {
	topology_t *topology;
} thread_map_alg_init_t;

typedef struct thread_map_alg_map_t {
	// input
	comm_matrix_t *m_init;

	// output
	uint32_t *map;
} thread_map_alg_map_t;

extern void (*libmapping_mapping_algorithm_map) (thread_map_alg_map_t*);
extern void (*libmapping_mapping_algorithm_destroy) (void*);

void* libmapping_mapping_algorithm_setup(topology_t *topology, char *alg);

#define LM_TMAP(label, fini, fmap, fdestroy) \
	void* fini (thread_map_alg_init_t *data);\
	void fmap (thread_map_alg_map_t *data);\
	void fdestroy (void *data);
#include "mapping-algorithms-list.h"
#undef LM_TMAP

void network_generate_groups (comm_matrix_t *m, uint32_t ntasks, machine_task_group_t *groups, uint32_t nmachines);
void network_map_groups_to_machines (machine_task_group_t *groups, machine_t *machines, uint32_t nmachines);
void network_generate_groups_load (comm_matrix_t *m, uint32_t ntasks, machine_t *machines, uint32_t nmachines, double *loads);

#endif
