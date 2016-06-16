#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <sys/time.h>
#include <ctype.h>

#include "libmapping.h"

machine_t *machines = NULL;
int nmachines = 0;

machine_t* get_machine_by_name (char *name)
{
	int i;
	
	for (i=0; i<nmachines; i++) {
		if (!strcmp(machines[i].name, name))
			return &machines[i];
	}
	
	return NULL;
}

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

#define READ_STR(STR) {\
		char *i;\
		if (!isalpha(*s)) {\
			printf("n eh uma letra code %u pos %u\n", (uint32_t)*s, (uint32_t)(s-buffer));\
			assert(0);\
		}\
		i = STR;\
		*i = *s;\
		s++;\
		i++;\
		while (isalnum(*s)) {\
			*i = *s;\
			s++;\
			i++;\
		}\
		*i = 0;\
	}

#define SKIP_SPACE \
	while (*s == ' ' || *s == '\t') {\
		INCS\
	}

#define FORCE_SKIP_SPACE \
	if (!(*s == ' ' || *s == '\t')) {\
		printf("require space\n");\
		assert(0);\
	}\
	SKIP_SPACE

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
	char NUMBER[100];
	
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

/*
	machine file format:
	
	number_of_machines
	(machine_name[i] arities_mem_hierarchy)*
	(i arity (j latency)*)*
*/

static void parse_machines (char *buffer, uint32_t blen)
{
	char *s, *p;
	char NUMBER[100];
	char str[100];
	char arities_str[100];
	machine_t *m, *m2;
	uint32_t arities[50];
	int i;
	int check_links;
	uint64_t v;
	
	s = buffer;
	
	SKIP_SPACE
	READ_STR(str)
	if (strcmp(str, "machines")) {
		printf("needs to start with the machines\n");
		exit(1);
	}
	SKIP_SPACE
	if (*s != ':') {
		printf("needs : after the machine label\n");
		exit(1);
	}
	INCS
	SKIP_SPACE
	SKIP_NEWLINE
	
	nmachines = 0;
	
	while (*s) {
		check_links = 0;
		
		SKIP_SPACE
		if (*s == "\n") {
			INCS
			continue;
		}
		
		READ_STR(str)
		if (*s == ':')
			check_links = 1;
		else {
			FORCE_SKIP_SPACE
		}
		if (*s == ':')
			check_links = 1;
		if (check_links && !strcmp(str, "links")) {
			INCS
			SKIP_SPACE
			SKIP_NEWLINE
			break;
		}

		nmachines++;
		machines = (machine_t*)realloc(machines, nmachines*sizeof(machine_t));
		assert(machines != NULL);
		m = &machines[nmachines-1];
		strcpy(m->name, str);

		p = arities_str;
		while (isdigit(*s) || *s == ',') {
			*p = *s;
			p++;
			INCS
		}
		*p = 0;
		
		m->id = nmachines - 1;

		m->topology.n_levels = to_vector(arities_str, arities, 50);
		m->topology.arities = malloc(sizeof(uint32_t) * m->topology.n_levels);
		assert(m->topology.arities != NULL);
		memcpy(m->topology.arities, arities, sizeof(uint32_t) * m->topology.n_levels);
		
		printf("machine %s: nlevels %i arities ", m->name, m->topology.n_levels);
		
		for (i=0; i<m->topology.n_levels; i++)
			printf("%u,", m->topology.arities[i]);
		
		printf("\n");
		
		SKIP_SPACE
		SKIP_NEWLINE
	}
	
	while (*s) {
		SKIP_SPACE
		if (*s == "\n") {
			INCS
			continue;
		}
		
		READ_STR(str)
		m = get_machine_by_name(str);
		assert(m != NULL);
		
		SKIP_SPACE
		
		READ_STR(str)
		m2 = get_machine_by_name(str);
		assert(m2 != NULL);
		
		SKIP_SPACE
		
		READ_INT(v)
		
		printf("link %s <---> %s (%llu)\n", m->name, m2->name, v);
		
		SKIP_SPACE
		
		if (*s) {
			SKIP_NEWLINE
		}
	}
exit(0);
}

int main(int argc, char **argv)
{
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
	
	uint32_t arities[50];
	
	if (argc != 3) {
		printf("Usage: %s <csv file> <machine file>\n", argv[0]);
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
	
	fp = fopen(argv[2], "r");
	assert(fp != NULL);
	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	buffer = (char*)malloc(fsize+1);
	assert(buffer != NULL);
	rewind(fp);
	assert( fread(buffer, sizeof(char), fsize, fp) == fsize );
	fclose(fp);
	buffer[fsize] = 0;

	parse_machines(buffer, fsize);
	free(buffer);

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
