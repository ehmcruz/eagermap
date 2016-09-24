#include "libmapping.h"

uint64_t libmapping_get_next_power_of_two (uint64_t v)
{
	uint64_t r;
	for (r=1; r<v; r<<=1);
	return r;
}

uint8_t libmapping_get_log2(uint64_t v)
{
	uint8_t r;
	for (r=0; (1 << r)<v; r++);
	return r;
}

void* libmapping_matrix_malloc(uint32_t nrows, uint32_t ncols, uint32_t type_size)
{
	void **p;
	uint32_t i;

	p = (void**)lm_calloc(nrows, sizeof(void*));
	assert(p != NULL);

	p[0] = (void*)lm_calloc(nrows*ncols, type_size);
	assert(p[0] != NULL);
	
	for (i=1; i<nrows; i++)
		p[i] = p[0] + i*ncols*type_size;

	return (void*)p;
}

void libmapping_matrix_free(void *m)
{
	void **p = (void**)m;
	lm_free(p[0]);
	lm_free(p);
}

void libmapping_comm_matrix_init (comm_matrix_t *m, uint32_t nthreads)
{
	m->nthreads = nthreads;
	assert(nthreads <= MAX_THREADS);
	
	m->max = libmapping_get_next_power_of_two(nthreads);
	m->bits = libmapping_get_log2(m->max);	
/*	printf("m->bits %i m->max %i\n", m->bits, m->max);exit(1);*/

	m->matrix = lm_calloc(m->max * m->max, sizeof(uint64_t));
	assert(m->matrix != NULL);
}

char* libmapping_strtok(char *str, char *tok, char del, uint32_t bsize)
{
	char *p;
	uint32_t i;

	for (p=str, i=0; *p && *p != del; p++, i++) {
		LM_ASSERT(i < (bsize-1))
		*tok = *p;
		tok++;
	}

	*tok = 0;
	
	if (*p)
		return p + 1;
	else if (p != str)
		return p;
	else
		return NULL;
}

uint8_t libmapping_env_get_integer(char *envname, int32_t *value)
{
	char *p;
	
	p = getenv(envname);
	if (!p) {
		return 0;
	}

	if (value != NULL)
		*value = atoi(p);
	
	return 1;
}

