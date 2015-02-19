#ifndef __LIBMAPPING_LIB_H__
#define __LIBMAPPING_LIB_H__


#define lm_printf(...) printf(__VA_ARGS__)
#define lm_fprintf(out, ...) printf(__VA_ARGS__)

#define lm_calloc(count, tsize) malloc((count) * (tsize))
#define lm_free(p) free(p)


struct spcd_comm_matrix {
	uint64_t matrix[MAX_THREADS*MAX_THREADS];
	uint32_t nthreads;
};

typedef struct spcd_comm_matrix comm_matrix_t;

static inline
uint64_t get_matrix(struct spcd_comm_matrix *m, int i, int j)
{
	return i > j ? m->matrix[(i<<MAX_THREADS_BITS) + j] : m->matrix[(j<<MAX_THREADS_BITS) + i];
}

static inline
void set_matrix(struct spcd_comm_matrix *m, int i, int j, uint64_t v)
{
	if (i > j)
		m->matrix[(i << MAX_THREADS_BITS) + j] = v;
	else
		m->matrix[(j << MAX_THREADS_BITS) + i] = v;
}


#define comm_matrix_el(m, row, col) get_matrix(&(m), row, col)
#define comm_matrix_ptr_el(m, row, col) get_matrix(m, row, col)
#define comm_matrix_ptr_write(m, row, col, v) set_matrix(m, row, col, v)


void* libmapping_matrix_malloc (uint32_t nrows, uint32_t ncols, uint32_t type_size);
void libmapping_matrix_free (void *m);
void libmapping_comm_matrix_init (comm_matrix_t *m, uint32_t nthreads);
void libmapping_comm_matrix_destroy (comm_matrix_t *m);

uint64_t libmapping_get_next_power_of_two (uint64_t v);
uint8_t libmapping_get_log2 (uint64_t v);

#endif
