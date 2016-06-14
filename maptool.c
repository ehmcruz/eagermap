#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <sys/time.h>

#include "libmapping.h"

#define PRINTF_UINT64  "%" PRIu64

#define INCS \
	s++;\
	assert((s - buffer) <= blen);

#define READ_INT(V) {\
		char *i;\
		if (!(*s >= '0' && *s <= '9')) {\
			printf("n eh um numero char code %u pos %u\n", (uint32_t)*s, (uint32_t)(s-buffer));\
			assert(0);\
		}\
		i = NUMBER;\
		while (*s >= '0' && *s <= '9') {\
			*i = *s;\
			INCS\
			i++;\
		}\
		*i = 0;\
		sscanf(NUMBER, PRINTF_UINT64 , &V);\
	}

#define SKIP_SPACE \
	while (*s == ' ' || *s == '\t') {\
		INCS\
	}

#define SKIP_NEWLINE \
	assert(*s == '\n');\
	INCS

#define SKIP_COMMA \
	assert(*s == ',');\
	INCS

static uint32_t parse_csv (char *buffer, uint32_t blen, comm_matrix_t *m)
{
	char *s;
	uint32_t i, j, trow;
	uint64_t v;
	uint32_t nt;
	static char NUMBER[100];
	
	s = buffer;
	nt = 1;
	while (*s != '\n') {
		if (*s == ',')
			nt++;
		s++;
	}
	
	libmapping_comm_matrix_init(m, nt);
	
	s = buffer;
	
	trow = nt - 1;
	for (i=0; i<nt; i++) {
		for (j=0; j<nt; j++) {
			READ_INT(v)
			comm_matrix_ptr_write(m, trow, j, v);
			if (j < (nt-1)) {
				SKIP_COMMA
			}
		}
		if (i < (nt-1)) {
			SKIP_NEWLINE
			trow--;
		}
	}
	
	// assert we found the end of the file
	while (*s) {
		assert(*s == '\n' || *s == '\t' || *s == '\r' || *s == ' ');
		s++;
	}
	
	return nt;
}

/*
	machine file format:
	
	number_of_machines
	(machine_name[i] arities_mem_hierarchy)*
	(i arity (j latency)*)*
*/



static uint32_t to_vector(char *str, uint32_t *vec, uint32_t n)
{
	char tok[32], *p;
	uint32_t i;
	
	p = str;
	
	p = libmapping_strtok(p, tok, ',', 32);
	i = 0;
	
	while (p != NULL) {
		assert(i < n);
/*dprintf("token %s\n", tok);*/
		vec[i++] = atoi(tok);
		p = libmapping_strtok(p, tok, ',', 32);
	}
	
	return i;
}

int main(int argc, char **argv)
{
	uint32_t arities[50];
	static comm_matrix_t m;
	uint32_t i, j, nt, fsize;
	uint32_t nlevels=0, npus=0, nvertices=0, *threads_per_pu;
	weight_t *weights=NULL;
	FILE *fp;
	char *buffer;
	struct timeval timer_begin, timer_end;
	double elapsed;
	double quality;
	thread_map_alg_map_t mapdata;
	topology_t topo_, *topology;
	uint32_t *pus = NULL;
	static uint32_t map[MAX_THREADS];
	
	if (argc != 3) {
		printf("Usage: %s <csv file> <arities>\nImportant: arities start from the root node\n", argv[0]);
		return 1;
	}

	topology = &topo_;

	fp = fopen(argv[1], "r");
	assert(fp != NULL);
	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	buffer = (char*)malloc(fsize+1);
	assert(buffer != NULL);
	rewind(fp);
	assert( fread(buffer, sizeof(char), fsize, fp) == fsize );
	fclose(fp);
	buffer[fsize] = 0;

	nt = parse_csv(buffer, fsize, &m);
	free(buffer);

	LM_ASSERT(nt <= MAX_THREADS)

	nlevels = to_vector(argv[2], arities, 50);

	libmapping_get_n_pus_fake_topology(arities, nlevels, &npus, &nvertices);
	printf("Hardware topology with %u levels, %u PUs and %u vertices\n", nlevels, npus, nvertices);

	weights = NULL;
	
	topology->pu_number = npus;
	libmapping_graph_init(&topology->graph, nvertices, nvertices-1);
	topology->root = libmapping_create_fake_topology(topology, arities, nlevels, pus, weights);
	topology->root->weight = 0;
	topology->root->type = GRAPH_ELTYPE_ROOT;
	
	libmapping_topology_analysis(topology);
	
	threads_per_pu = (uint32_t*)malloc(topology->pu_number);
	LM_ASSERT(threads_per_pu != NULL)
	
/*	for (i=0; i<MAX_THREADS; i++) {*/
/*		libmapping_threads[i].stat = THREAD_DEAD;*/
/*	#ifdef LIBMAPPING_STATS_THREAD_LOAD*/
/*		libmapping_threads[i].time_start = 0;*/
/*		libmapping_threads[i].time_end = 1;*/
/*		libmapping_threads[i].time_running = 1;*/
/*	#endif*/
/*		libmapping_alive_threads[i] = &libmapping_threads[i];*/
/*	}*/

{
	thread_map_alg_init_t init;
	init.topology = topology;
	libmapping_mapping_algorithm_greedy_init(&init);
}

	mapdata.m_init = &m;
	mapdata.map = map;
	
	gettimeofday(&timer_begin, NULL);
	libmapping_mapping_algorithm_greedy_map(&mapdata);
	gettimeofday(&timer_end, NULL);
	
	for (i=0; i<topology->pu_number; i++) {
		threads_per_pu[i] = 0;
	}
	for (i=0; i<nt; i++) {
		threads_per_pu[ map[i] ]++;
	}
	
	quality = 0.0;
	for (i=0; i<nt-1; i++) {
		for (j=i+1; j<nt; j++) {
/*			printf("el=%u, map[i]=%u, map[j]=%u, dist %u\n", comm_matrix_el(m, i, j), map[i], map[j], libmapping_topology_dist_pus(topology, map[i], map[j]));*/
			quality += comm_matrix_el(m, i, j) / (libmapping_topology_dist_pus(topology, map[i], map[j]) + 1.0);
		}
	}

	elapsed = timer_end.tv_sec - timer_begin.tv_sec + (timer_end.tv_usec - timer_begin.tv_usec) / 1000000.0;

	printf("Number of tasks in communication matrix: %u\n", nt);

	printf("Tasks per PU: ");
	for (i=0; i<topology->pu_number; i++){
		printf("%d", threads_per_pu[i]);
		if (i!=topology->pu_number-1)
			printf(",");
	}
	printf("\n");

	printf("Execution time of the algorithm: %.4fms\n", elapsed*1000.0);
	printf("Mapping quality (higher is better): %.4f\n", quality);

	printf("Mapping: ");
	for (i=0; i<nt; i++){
		printf("%d", map[i]);
		if (i < (nt-1))
			printf(",");
	}
	printf("\n");
	
	return 0;
}
