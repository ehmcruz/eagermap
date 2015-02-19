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

	p[0] = (void*)lm_calloc(nrows*ncols, type_size);
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
