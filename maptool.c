#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "libmapping.h"


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
	printf("matrix malloc %u threads\n", nt);
	
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

int main(int argc, char **argv)
{
	uint32_t arities[50];
	static uint32_t forced_mapping_n = 0;
	static comm_matrix_t m;
	uint32_t i, j, nt, fsize;
	uint32_t nlevels, npus=0, nvertices, *threads_per_pu;
	weight_t *weights;
	FILE *fp;
	char *buffer;
	struct timeval timer_begin, timer_end;
	double elapsed;
	double quality;
	void *alg;
	thread_map_alg_map_t mapdata;
	topology_t *topology;
	static const char *topofrom[2] = { "native", "fake" };
	static uint32_t map[MAX_THREADS];
	
	if (argc < 3  || argc > 7) {
		printf("%s csv algorithm [arities] [weights]\nImportant: arities/weights starting from the root node\n", argv[0]);
		return 1;
	}

	topology = libmapping_topology_get();

	printf("hello world file %s topo from %s\n", argv[1], topofrom[argc == 4]);
	
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
/*	libmapping_print_matrix(&m, stdout);*/

	LM_ASSERT(nt <= MAX_THREADS)

	if (argc >= 4) {
		uint32_t *pus = NULL;
		
		nlevels = to_vector(argv[3], arities, 50);

		libmapping_get_n_pus_fake_topology(arities, nlevels, &npus, &nvertices);

		if (argc >= 5) {
			uint32_t w[50], n;
			static weight_t ww[50];
						
			n = to_vector(argv[4], w, 50);
			LM_ASSERT(n == nlevels)
			
			weights = ww;
			
			for (i=0; i<n; i++)
				weights[i] = w[i];
		}
		else
			weights = NULL;
			
		if (argc >= 6) {
			forced_mapping_n = 0;
			printf("with forced mapping\n");
			forced_mapping_n = to_vector(argv[5], map, MAX_THREADS);
			LM_ASSERT(forced_mapping_n >= nt)
			printf("%u threads set map: ", forced_mapping_n);
			for (i=0; i<nt; i++) {
				printf("%u,", map[i]);
				// for (j=i+1; j<nt; j++) {
				// 	LM_ASSERT(map[i] != map[j])
				// }
			}
			printf("\n");
		}
		
		if (argc >= 7) {
			static uint32_t pu_ids[1024];
			uint32_t my_npus;
			
			printf("with forced pu ids ");
			my_npus = to_vector(argv[6], pu_ids, 1024);
			LM_ASSERT(npus == my_npus)
			pus = pu_ids;

			for (i=0; i<npus; i++) {
				printf("%u,", pus[i]);
				for (j=i+1; j<npus; j++) {
					LM_ASSERT(pus[i] != pus[j])
				}
			}
			printf("\n");
		}
		
		topology->pu_number = npus;
		libmapping_graph_init(&topology->graph, nvertices, nvertices-1);
		topology->root = libmapping_create_fake_topology(arities, nlevels, pus, weights);
		topology->root->weight = 0;
		topology->root->type = GRAPH_ELTYPE_ROOT;
		
		libmapping_topology_analysis(topology);
	}
	else {
		libmapping_topology_init();
	}
	
	threads_per_pu = (uint32_t)malloc(topology->pu_number);
	LM_ASSERT(threads_per_pu != NULL)
	
	for (i=0; i<MAX_THREADS; i++) {
		libmapping_threads[i].stat = THREAD_DEAD;
	#ifdef LIBMAPPING_STATS_THREAD_LOAD
		libmapping_threads[i].time_start = 0;
		libmapping_threads[i].time_end = 1;
		libmapping_threads[i].time_running = 1;
	#endif
		libmapping_alive_threads[i] = &libmapping_threads[i];
	}

	if (forced_mapping_n == 0)
		alg = libmapping_mapping_algorithm_setup(topology, argv[2]);
	printf("topology set\n");

	mapdata.foo = alg;
	mapdata.m_init = &m;
	mapdata.map = map;
	mapdata.alive_threads = libmapping_alive_threads;
	
	gettimeofday(&timer_begin, NULL);
	if (forced_mapping_n == 0)
		libmapping_mapping_algorithm_map(&mapdata);
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

	printf("Number of threads: %u\n", nt);
	printf("MAP ");
	for (i=0; i<nt; i++){
		printf("%d", map[i]);
		if (i < (nt-1))
			printf(",");
	}
	printf("\n\n");
	printf("threads per pu: ");
	for (i=0; i<topology->pu_number; i++){
		printf("%d,", threads_per_pu[i]);
	}
	printf("\n");
	printf("time: %.4fms\n", elapsed*1000.0);
	printf("quality (higher is better): %.4f\n", quality);
	
	return 0;
}
