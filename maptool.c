#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <sys/time.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "libmapping.h"

#define ENABLE_LOAD_BALANCE

#define DIST_DIFFERENT_MACHINES 1000

typedef struct map_t {
	machine_t *machine;
	uint32_t pu;
	uint32_t pu_pos;
} map_t;

typedef struct pu_machine_t {
	int id;
	machine_t *machine;
	int pu_pos;
} pu_machine_t;

static pu_machine_t *pus;
static int total_pus;

static machine_t *machines = NULL;
static machine_task_group_t *groups;
static int nmachines = 0;
static int use_load;
static int provided_load;
static double *loads = NULL;

static comm_matrix_t m;

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
	if (*s != '\n') {\
		printf("faltou newline\n");\
		assert(0);\
	}\
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

static void generate_nn_matrix (int nt, comm_matrix_t *m)
{
	int i, j, diff;
	uint64_t max, comm;

	if (nt > 16)
		max = 1 << 16;
	else
		max = 1 << nt;

	libmapping_comm_matrix_init(m, nt);

	for (i=nt-1; i>=0; i--) {
		for (j=0; j<nt; j++) {
			diff = i - j;
			if (diff < 0)
				diff *= -1;
			if (i == j)
				comm = 0;
			else if (diff == 1)
				comm = max;
			else
				comm = max >> diff;
			comm_matrix_ptr_write(m, i, j, comm);
		}
	}
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
	int i, j;
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
		if (*s == '\n') {
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

		SKIP_SPACE
		SKIP_NEWLINE
	}

	for (i=0; i<nmachines; i++) {
		m = &machines[i];

		m->id = i;
		m->links = NULL;
		m->nlinks = 0;

		printf("machine-%i %s: nlevels %i npus_check %i arities ", m->id, m->name, m->topology.n_levels, m->npus_check);

		for (j=0; j<m->topology.n_levels; j++)
			printf("%u,", m->topology.arities[j]);

		printf(" pus ");

		for (j=0; j<m->npus_check; j++)
			printf("%u,", m->best_pus[j]);

		printf("\n");

	}

	while (*s) {
		SKIP_SPACE
		if (*s == '\n') {
			INCS
			continue;
		}

		READ_STR(str)
		m = get_machine_by_name(str);

		if (m == NULL) {
			printf("can't find machine %s\n", str);
			exit(1);
		}

		SKIP_SPACE

		READ_STR(str)
		m2 = get_machine_by_name(str);

		if (m2 == NULL) {
			printf("can't find machine %s\n", str);
			exit(1);
		}

		SKIP_SPACE

		READ_INT(v)

		printf("link %s <---> %s (%lu)\n", m->name, m2->name, v);

		m->links = (machine_link_t*)realloc(m->links, (m->nlinks+1)*sizeof(machine_link_t));
		assert(m->links != NULL);

		for (i=0; i<m->nlinks; i++) {
			if (m->links[i].machine == m2) {
				printf("double link between machines %s and %s\n", m->name, m2->name);
				exit(1);
			}
		}

		m->links[ m->nlinks ].machine = m2;
		m->links[ m->nlinks ].weight = v;
		m->nlinks++;

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
		printf("load[%i] = %f\n", i, loads[i]);

		if (i < (nt-1)) {
			FORCE_SKIP_SPACE_NEWLINE

			if (*s == 0) {
				printf("we need %i loads in the load file (given %i)\n", nt, i+1);
				exit(1);
			}

		}
	}
}

static void parse_scotch_map (char *buffer, uint32_t blen, map_t *map, uint32_t nt)
{
	int i, task, pu;
	char *s, *p;
	char NUMBER[100];
	uint64_t v;
	
	printf("parsing scotch map...\n");

	s = buffer;

	READ_INT(v)
	SKIP_NEWLINE
	
	if (nt != (uint32_t)v) {
		printf("error: the number of tasks in scotch map (%i) is different from the number of tasks of the comm matrix (%i)\n", (uint32_t)v, nt);
		exit(1);
	}
	
	printf("scotch number of threads: %i\n", nt);
	
	for (i=0; i<nt; i++) {
		READ_INT(v)
		task = v;
		
		FORCE_SKIP_SPACE
		
		READ_INT(v)
		pu = v;
		
		SKIP_NEWLINE
		
		if (pu >= total_pus) {
			printf("the pu id %i in scotch map file in task %i is higher than he total number of pus in the system (%u)\n", pu, task, total_pus);
			exit(1);
		}
		
		map[task].machine = pus[pu].machine;
		map[task].pu_pos = pus[pu].pu_pos;
		map[task].pu = pus[pu].machine->best_pus[ pus[pu].pu_pos ];
		
		printf("scotch map task %i on machine %s pu %i\n", task, map[task].machine->name, map[task].pu);
	}
}

static void generate_full_load (int nt)
{
	int i;
	
	loads = malloc(nt * sizeof(double));
	assert(loads != NULL);

	for (i=0; i<nt; i++)
		loads[i] = 1.0;
}

static void normalize_load (int nt)
{
	int i;
	double max;
	
	printf("normalizing loads...\n");
	
	max = loads[0];
	for (i=1; i<nt; i++) {
		if (loads[i] > max)
			max = loads[i];
	}
	
	for (i=0; i<nt; i++) {
		loads[i] /= max;
		printf("load[%i] = %.3f\n", i, loads[i]);
	}
}

static void convert_topo_to_scotch_graph (char *fname)
{
	FILE *fp;
	int i, nedges, j, comm;
		
	fp = fopen(fname, "w");
	assert(fp != NULL);
	
	fprintf(fp, "0\n");
	
	// 1 vertex per objects (root, cache, pu) of all machines
	// an edge per pair of pu
	nedges = total_pus * (total_pus-1);
	
	printf("nedges: %i\n", nedges);
	
	fprintf(fp, "%i %i\n", total_pus, nedges);

	fprintf(fp, "0 010\n");
		
	for (i=0; i<total_pus; i++) {
		fprintf(fp, "%i", total_pus-1);
		
		for (j=0; j<total_pus; j++) {
			if (i != j) {
/*printf("i %i pus[i].machine_id %i pus[i].pu_pos %i |   j %i pus[j].machine_id %i pus[j].pu_pos %i\n", i, pus[i].machine_id, pus[i].pu_pos, j, pus[j].machine_id, pus[j].pu_pos);*/
				if (pus[i].machine->id == pus[j].machine->id)
					comm = libmapping_topology_dist_pus(&pus[i].machine->topology, pus[i].pu_pos, pus[j].pu_pos);
				else
					comm = DIST_DIFFERENT_MACHINES;
			
				fprintf(fp, " %i %i", comm, j);
			}
		}
		
		fprintf(fp, "\n");
	}
		
	fclose(fp);
	
	printf("scotch topology printed in file %s\n", fname);
}

static void convert_matrix_to_scotch_graph (comm_matrix_t *m, char *fname)
{
	FILE *fp;
	int nedges, i, j;
	
	fp = fopen(fname, "w");
	assert(fp != NULL);
	
	fprintf(fp, "0\n");

	// an edge per pair of pu
	nedges = m->nthreads * (m->nthreads-1);
	
	printf("nthreads: %i\n", m->nthreads);
	printf("nedges: %i\n", nedges);

	fprintf(fp, "%u %i\n", m->nthreads, nedges);

	fprintf(fp, "0 010\n");

	for (i=0; i<m->nthreads; i++) {
		fprintf(fp, "%u", m->nthreads - 1);

		for (j=0; j<m->nthreads; j++) {
			if (j != i)
				fprintf(fp, " %llu %i", comm_matrix_ptr_el(m, i, j), j);
		}

		fprintf(fp, "\n");
	}

	fclose(fp);
	
	printf("scotch comm matrix printed in file %s\n", fname);
}

static double gen_map_quality (map_t *map, int nt)
{
	double quality, dist;
	int i, j;

	quality = 0.0;

	for (i=0; i<nt-1; i++) {
		for (j=i+1; j<nt; j++) {
			if (map[i].machine == map[j].machine)
				dist = (double)libmapping_topology_dist_pus(&map[i].machine->topology, map[i].pu_pos, map[j].pu_pos);
			else
				dist = (double)DIST_DIFFERENT_MACHINES;
/*printf("tasks %i-%i comm %llu dist %.3f\n", i, j, comm_matrix_el(m, i, j), dist);getchar();*/
			quality += (double)comm_matrix_el(m, i, j) / (dist + 1.0);
		}
	}

	return quality;
}

static void generate_random_mapping (map_t *map, uint32_t nt)
{
	uint32_t i, j;
	int *avl, navl, pu, pos;
	
	srand(time(NULL));
	
	avl = malloc(total_pus * sizeof(int));
	assert(avl != NULL);
	
	navl = 0;
	
	for (i=0; i<nt; i++) {
		if (!navl) {
			navl = total_pus;
			
			for (j=0; j<total_pus; j++)
				avl[j] = j;
		}
		
		pos = rand() % navl;
		pu = avl[pos];
		
		map[i].machine = pus[pu].machine;
		map[i].pu_pos = pus[pu].pu_pos;
		map[i].pu = pus[pu].machine->best_pus[ pus[pu].pu_pos ];
		
		navl--;
		avl[pos] = avl[navl];
	}
	
	free(avl);
}

static void print_mapping (map_t *map, uint32_t nt)
{
	uint32_t i;
	double quality;

	for (i=0; i<nt; i++) {
		printf("rank %i=%s slot=%i\n", i, map[i].machine->name, map[i].pu);
	}

	printf("single machine mapping: ");

	for (i=0; i<nt; i++) {
		printf("%i", map[i].pu);
		if (i < (nt-1))
			printf(",");
	}

	printf("\n");
	
	quality = gen_map_quality(map, nt);
	printf("quality of mapping: %.3f\n", quality);
}

static void display_usage (int argc, char **argv)
{
	printf("Usage:\n");
	printf("\t%s csv_file[-n_] machine_file [load_file][-f] [-norm] [-pscotch] [-mscotch scotch_map_file] [-rand]\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	uint32_t i, j, k, nt, fsize;
	uint32_t nlevels=0, npus=0, nvertices=0, *threads_per_pu;
	weight_t *weights=NULL;
	FILE *fp;
	char *buffer, *fname_csv, *fname_load, *fname_topo, *fname_scotch_map;
	struct timeval timer_begin, timer_end;
	double elapsed;
	double quality;
	double machine_load, pu_load;
	thread_map_alg_map_t mapdata;
	machine_t *machine;
	thread_map_alg_init_t init;
	map_t *map;
	int norm, args_normal, print_scotch, eval_scotch_map, random_mapping;
	
	printf("compiled to support up to %i threads\n", MAX_THREADS);

	provided_load = 0;
	use_load = 0;
	norm = 0;
	args_normal = 0;
	print_scotch = 0;
	eval_scotch_map = 0;
	random_mapping = 0;

	i = 1;
	while (i < argc) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "-norm")) {
				norm = 1;
				i++;
			}
			else if (!strcmp(argv[i], "-pscotch")) {
				print_scotch = 1;
				i++;
			}
			else if (!strcmp(argv[i], "-load")) {
				use_load = 1;
				i++;
			}
			else if (!strcmp(argv[i], "-rand")) {
				random_mapping = 1;
				i++;
			}
			else if (!strcmp(argv[i], "-mscotch")) {
				i++;
				
				if (i == argc || argv[i][0] == '-') {
					printf("need scotch mapping file after -mscotch\n");
					display_usage(argc, argv);
				}
				
				fname_scotch_map = argv[i];
				
				eval_scotch_map = 1;
				
				i++;
			}
			else if (argv[i][1] == 'n' && args_normal == 0) { // automatically generate nearest neightbor matrix
				char *number, *s;
				int error = 0;
				
				fname_csv = argv[i];

				number = fname_csv+2;

				if (!(*number))
					error = 1;

				for (s=number; *s; s++) {
					if (!isdigit(*s))
						error = 1;
				}

				if (error) {
					printf("autogen comm matrix nearest neightbor error: we need a number after -n, given %s\n", number);
					exit(1);
				}

				nt = atoi(number);
				printf("autogen comm matrix nearest neightbor number of threads: %i\n", nt);

				if (!nt) {
					printf("autogen comm matrix nearest neightbor error: we need a number of threads higher than 0\n");
					exit(1);
				}

				assert(nt <= MAX_THREADS);

				generate_nn_matrix(nt, &m);
				
				args_normal++;
				i++;
			}
			else if (argv[i][1] == 'f' && args_normal == 2) { // automatically generate full load
				printf("autogen full load\n");
				provided_load = 1;
				generate_full_load(nt);
				
				args_normal++;
				i++;
			}
			else
				display_usage(argc, argv);
		}
		else {
			if (args_normal == 0) { // parse csv file as comm matrix
				fname_csv = argv[i];
				
				printf("csv input matrix: %s\n", fname_csv);
				
				fp = fopen(fname_csv, "r");
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
				
				args_normal++;
				i++;
			}
			else if (args_normal == 1) { // parse topo
				fname_topo = argv[i];
				printf("topology input: %s\n", fname_topo);
				
				fp = fopen(fname_topo, "r");
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
				
				args_normal++;
				i++;
			}
			else if (args_normal == 2) { // parse load
				fname_load = argv[i];
				
				printf("load input file: %s\n", fname_load);
				
				provided_load = 1;
				
				fp = fopen(fname_load, "r");
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
				
				args_normal++;
				i++;
			}
			else
				display_usage(argc, argv);
		}
	}
	
	if (args_normal < 2)
		display_usage(argc, argv);
	
	if (use_load && !provided_load) {
		printf("error! given -load, but load file not provided\n");
		display_usage(argc, argv);
	}

	if (use_load)
		printf("balance the load\n");
	else
		printf("do not balance the load\n");

	if (use_load && norm)
		normalize_load(nt);

	groups = malloc(nmachines * sizeof(machine_task_group_t));
	assert(groups != NULL);

	map = malloc(sizeof(map_t) * nt);
	assert(map != NULL);

	printf("Number of threads: %i\n", nt);
	printf("Number of machines: %i\n", nmachines);

	for (i=0; i<nmachines; i++) {
		machine = &machines[i];

		npus = 0;
		nvertices = 0;
		libmapping_get_n_pus_fake_topology(machine->topology.arities, machine->topology.n_levels, &npus, &nvertices);

		weights = NULL;

		machine->topology.pu_number = npus;
		libmapping_graph_init(&machine->topology.graph, nvertices, nvertices-1);
		machine->topology.root = libmapping_create_fake_topology(&machine->topology, machine->topology.arities, machine->topology.n_levels, NULL, weights);
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

	total_pus = 0;
	
	for (i=0; i<nmachines; i++)
		total_pus += machines[i].topology.pu_number;

	printf("total_pus: %i\n", total_pus);
	
	pus = malloc(total_pus * sizeof(struct pu_machine_t));
	assert(pus != NULL);
	
	k = 0;
	for (i=0; i<nmachines; i++) {
		for (j=0; j<machines[i].topology.pu_number; j++) {
			pus[k].id = k;
			pus[k].machine = &machines[i];
			pus[k].pu_pos = j;
			k++;
		}
	}
	
	assert(k == total_pus);

	network_floyd_warshall(machines, nmachines);
	
	if (print_scotch) {
		printf("print scotch mode\n");
		convert_topo_to_scotch_graph("scotch-topology.grf");
		convert_matrix_to_scotch_graph(&m, "scotch-comm-matrix.grf");
		
		return 0;
	}
	
	if (eval_scotch_map) {
		printf("evaluate scotch mapping %s\n", fname_scotch_map);
		
		fp = fopen(fname_scotch_map, "r");
		assert(fp != NULL);
		fseek(fp, 0, SEEK_END);
		fsize = ftell(fp);
		buffer = (char*)malloc(fsize+1);
		assert(buffer != NULL);
		rewind(fp);
		assert( fread(buffer, sizeof(char), fsize, fp) == fsize );
		fclose(fp);
		buffer[fsize] = 0;

		parse_scotch_map(buffer, fsize, map, nt);
		free(buffer);
	}
	else if (random_mapping) {
		printf("using random mapping\n");
		
		gettimeofday(&timer_begin, NULL);
		generate_random_mapping(map, nt);
		gettimeofday(&timer_end, NULL);
	}
	else {	
		init.nt = nt;
	
		if (!use_load) {
			for (i=0; i<nmachines; i++) {
				init.topology = &machines[i].topology;
				libmapping_mapping_algorithm_greedy_init(&init);
			}
		}
		else {
			for (i=0; i<nmachines; i++) {
				init.topology = &machines[i].topology;
				libmapping_mapping_algorithm_greedy_lb_init(&init);
			}
		}

		for (i=0; i<nmachines; i++) {
			for (j=0; j<MAX_THREADS; j++) {
				machines[i].map[j] = -1;
			}
		}

		printf("calculating mapping...\n");

		gettimeofday(&timer_begin, NULL);

		if (!use_load) {
			network_generate_groups(&m, nt, groups, nmachines);

			network_map_groups_to_machines(groups, machines, nmachines);

	/*		for (i=0; i<nmachines; i++) {*/
	/*			printf("machine %i: %i tasks -> ", i, machines[i].ntasks);*/
	/*		*/
	/*			for (j=0; j<machines[i].ntasks; j++) {*/
	/*				printf("%i,", machines[i].tasks[j]);*/
	/*			}*/
	/*		*/
	/*			printf("\n");*/
	/*		}*/
	/*printf("blah\n");*/
			for (i=0; i<nmachines; i++) {
				mapdata.m_init = machines[i].cm;
				mapdata.map = machines[i].map;

				libmapping_mapping_algorithm_greedy_map(&mapdata);
			}
		}
		else {
			network_generate_groups_load(&m, nt, groups, nmachines, loads);

			network_map_groups_to_machines(groups, machines, nmachines);

	/*		for (i=0; i<nmachines; i++) {*/
	/*			printf("machine %i: %i tasks -> ", i, machines[i].ntasks);*/
	/*		*/
	/*			for (j=0; j<machines[i].ntasks; j++) {*/
	/*				printf("%i,", machines[i].tasks[j]);*/
	/*			}*/
	/*		*/
	/*			printf("\n");*/
	/*		}*/

			for (i=0; i<nmachines; i++) {
				mapdata.m_init = machines[i].cm;
				mapdata.map = machines[i].map;
				mapdata.loads = loads;

				libmapping_mapping_algorithm_greedy_lb_map(&mapdata);
			}
		}

		gettimeofday(&timer_end, NULL);

	/*printf("blah\n");*/
	/*exit(1);*/
	/*for (i=0; i<nmachines; i++) {*/
	/*	for (j=0; j<machines[i].ntasks; j++) {*/
	/*		printf("i %i, j %i, machines[i](%s).map[j] %i\n", i, j, machines[i].name, machines[i].map[j]);*/
	/*	}*/
	/*}*/


		for (i=0; i<nt; i++)
			map[i].machine = NULL;

		for (i=0; i<nmachines; i++) {
			for (j=0; j<machines[i].ntasks; j++) {
				assert(machines[i].tasks[j] < nt);
	/*			printf("i %i, j %i, machines[i].map[j] %i\n", i, j, machines[i].map[j]);*/
				map[ machines[i].tasks[j] ].machine = &machines[i];
				map[ machines[i].tasks[j] ].pu = machines[i].best_pus[ machines[i].map[j] ];
				map[ machines[i].tasks[j] ].pu_pos = machines[i].map[j];
			}
		}

		for (i=0; i<nt; i++) {
			assert(map[i].machine != NULL);
		}

		if (provided_load) {
			printf("loads per machine:\n");

			for (i=0; i<nmachines; i++) {
				machine_load = 0.0;
				for (j=0; j<machines[i].ntasks; j++)
					machine_load += loads[ machines[i].tasks[j] ];
				printf("%s (%.3f): ", machines[i].name, machine_load);

	/*			machine_load = 0.0;*/
				for (j=0; j<machines[i].topology.pu_number; j++) {
					pu_load = 0.0;

					for (k=0; k<machines[i].ntasks; k++) {
						if (machines[i].map[k] == j)
							pu_load += loads[ machines[i].tasks[k] ];
					}

					printf("%.3f ", pu_load);

	/*				machine_load += pu_load;*/
				}

	/*			printf("--> %.3f", machine_load);*/
				printf("\n");
			}
		}
	}
	
	print_mapping(map, nt);

	elapsed = timer_end.tv_sec - timer_begin.tv_sec + (timer_end.tv_usec - timer_begin.tv_usec) / 1000000.0;
	printf("mapping time: %.5f s\n", elapsed);

	return 0;
}
