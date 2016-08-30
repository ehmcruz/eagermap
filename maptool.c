#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <sys/time.h>
#include <ctype.h>

#include "libmapping.h"

typedef struct map_t {
	machine_t *machine;
	uint32_t pu;
} map_t;

static machine_t *machines = NULL;
static machine_task_group_t *groups;
static int nmachines = 0;
static int use_load;
static double *loads = NULL;

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
		printf("require space (given hex 0x%X)\n", (unsigned)*s);\
		assert(0);\
	}\
	SKIP_SPACE

#define FORCE_SKIP_SPACE_NEWLINE \
	if (!(*s == ' ' || *s == '\t' || *s == '\n')) {\
		printf("require space (given hex 0x%X)\n", (unsigned)*s);\
		assert(0);\
	}\
	while (*s == ' ' || *s == '\t' || *s == '\n') {\
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
	char *arities_str, *pus_str;
	machine_t *m, *m2;
	uint32_t arities[50];
	int i;
	int check_links;
	uint64_t v;
	
	arities_str = malloc(1024);
	assert(arities_str != NULL);
	pus_str = malloc(10*1024);
	assert(pus_str != NULL);

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
		
		FORCE_SKIP_SPACE
		
		p = pus_str;
		while (isdigit(*s) || *s == ',') {
			*p = *s;
			p++;
			INCS
		}
		*p = 0;
		
		m->best_pus = malloc(sizeof(uint32_t) * 1024);
		assert(m->best_pus != NULL);
		
		m->npus_check = to_vector(pus_str, m->best_pus, 1024);
		
		printf("machine %s: nlevels %i npus_check %i arities ", m->name, m->topology.n_levels, m->npus_check);
		
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
	
	free(arities_str);
	free(pus_str);
}

static void parse_loads (char *buffer, uint32_t blen, int nt)
{
	int i;
	char *s, *p;
	char NUMBER[100];
	
	loads = malloc(nt * sizeof(double));
	assert(loads != NULL);
	
	printf("parsing load...\n");
	
	s = buffer;
	
	for (i=0; i<nt; i++) {
		if (!isdigit(*s)) {
			printf("only numbers are allowed in the load file\n");
			exit(1);
		}

		p = NUMBER;
		
		while (isdigit(*s)) {
			*p = *s;
			p++;
			INCS
		}
		if (*s == '.') {
			*p = *s;
			p++;
			INCS
			
			while (isdigit(*s)) {
				*p = *s;
				p++;
				INCS
			}
		}
		*p = 0;
		loads[i] = atof(NUMBER);
		printf("load %i %f\n", i, loads[i]);
		
		if (i < (nt-1)) {
			FORCE_SKIP_SPACE_NEWLINE
			
			if (*s == 0) {
				printf("we need %i loads in the load file (given %i)\n", nt, i+1);
				exit(1);
			}

		}
	}
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
	machine_t *machine;
	thread_map_alg_init_t init;
	map_t *map;
	
	if (argc != 3 && argc != 4) {
		printf("Usage: %s csv_file machine_file [load_file]\n", argv[0]);
		return 1;
	}
	
	use_load = (argc == 4);

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
	
	if (use_load) {
		fp = fopen(argv[3], "r");
		assert(fp != NULL);
		fseek(fp, 0, SEEK_END);
		fsize = ftell(fp);
		buffer = (char*)malloc(fsize+1);
		assert(buffer != NULL);
		rewind(fp);
		assert( fread(buffer, sizeof(char), fsize, fp) == fsize );
		fclose(fp);
		buffer[fsize] = 0;

		parse_loads(buffer, fsize, nt);
		free(buffer);
	}
	
	groups = malloc(nmachines * sizeof(machine_task_group_t));
	assert(groups != NULL);
	
	map = malloc(sizeof(map_t) * nt);
	assert(map != NULL);
	
	printf("Number of threads: %i\n", nt);
	
	for (i=0; i<nmachines; i++) {
		machine = &machines[i];
		
		npus = 0;
		nvertices = 0;
		libmapping_get_n_pus_fake_topology(machine->topology.arities, machine->topology.n_levels, &npus, &nvertices);

		weights = NULL;
	
		machine->topology.pu_number = npus;
		libmapping_graph_init(&machine->topology.graph, nvertices, nvertices-1);
		machine->topology.root = libmapping_create_fake_topology(&machine->topology, machine->topology.arities, machine->topology.n_levels, machine->best_pus, weights);
		machine->topology.root->weight = 0;
		machine->topology.root->type = GRAPH_ELTYPE_ROOT;
	
		libmapping_topology_analysis(&machine->topology);
		
		assert(machine->npus_check == machine->topology.pu_number);

		printf("Hardware topology with %u levels, %u PUs and %u vertices, pus: ", machine->topology.n_levels, machine->topology.pu_number, nvertices);
/*exit(1);*/

		for (j=0; j<machine->topology.pu_number; j++)
			printf("%i,", machine->topology.best_pus[j]);
		printf("\n");
		
		groups[i].id = i;
		groups[i].npus = machine->topology.pu_number;
	}
/*nt=128;*/
	gettimeofday(&timer_begin, NULL);

	if (!use_load) {
		network_generate_groups(&m, nt, groups, nmachines);
		
		network_map_groups_to_machines(groups, machines, nmachines);

		for (i=0; i<nmachines; i++) {
			printf("machine %i: %i tasks -> ", i, machines[i].ntasks);
		
			for (j=0; j<machines[i].ntasks; j++) {
				printf("%i,", machines[i].tasks[j]);
			}
		
			printf("\n");
		}
	
		for (i=0; i<nmachines; i++) {
			init.topology = &machines[i].topology;
			libmapping_mapping_algorithm_greedy_init(&init);

			mapdata.m_init = machines[i].cm;
			mapdata.map = machines[i].map;
	
			libmapping_mapping_algorithm_greedy_map(&mapdata);
		}
	}
	else {
		network_generate_groups_load(&m, nt, groups, nmachines, loads);
		
		network_map_groups_to_machines(groups, machines, nmachines);

		for (i=0; i<nmachines; i++) {
			printf("machine %i: %i tasks -> ", i, machines[i].ntasks);
		
			for (j=0; j<machines[i].ntasks; j++) {
				printf("%i,", machines[i].tasks[j]);
			}
		
			printf("\n");
		}
	
		for (i=0; i<nmachines; i++) {
			init.topology = &machines[i].topology;
			libmapping_mapping_algorithm_greedy_init(&init);

			mapdata.m_init = machines[i].cm;
			mapdata.map = machines[i].map;
	
			libmapping_mapping_algorithm_greedy_map(&mapdata);
		}
	}

	gettimeofday(&timer_end, NULL);
	
	for (i=0; i<nt; i++)
		map[i].machine = NULL;
	
	for (i=0; i<nmachines; i++) {
		for (j=0; j<machines[i].ntasks; j++) {
			assert(machines[i].tasks[j] < nt);
			map[ machines[i].tasks[j] ].machine = &machines[i];
			map[ machines[i].tasks[j] ].pu = machines[i].map[j];
		}
	}
	
	for (i=0; i<nt; i++) {
		assert(map[i].machine != NULL);
	}
	
	for (i=0; i<nt; i++) {
		printf("rank %i=%s slot=%i\n", i, map[i].machine->name, map[i].pu);
	}
	
	return 0;
}
