#ifndef __LIBMAPPING_MAPPING_ALGORITHMS_H__
#define __LIBMAPPING_MAPPING_ALGORITHMS_H__

typedef struct thread_map_alg_init_t {
	topology_t *topology;
} thread_map_alg_init_t;

typedef enum group_type_t {
	GROUP_TYPE_THREAD,
	GROUP_TYPE_GROUP
} group_type_t;

typedef struct thread_group_t {
	group_type_t type;
	uint32_t id;
	uint32_t nelements;
	double load;
	struct thread_group_t *elements[MAX_THREADS];
} thread_group_t;

typedef struct thread_map_alg_map_t {
	// input
	comm_matrix_t *m_init;
	thread_group_t **alive_threads;

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

#endif
