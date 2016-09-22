#include "libmapping.h"

static char chosen[MAX_THREADS];

static void network_generate_group_lb (comm_matrix_t *m, uint32_t ntasks, char *chosen, machine_task_group_t *group, double *loads, double max_load)
{
	weight_t w, wmax;
	uint32_t winner = 0;
	uint32_t i, j, k;

	group->ntasks = 0;
	group->load = 0.0;
	
	for (i=0; group->load<max_load; i++) { // in each iteration, find one element of the group
		wmax = -1;
		for (j=0; j<ntasks; j++) { // iterate over all elements to find the one that maximizes the communication relative to the elements that are already in the group
			if (!chosen[j]) {
				w = 0;
				for (k=0; k<i; k++) {
					w += comm_matrix_ptr_el(m, j, group->tasks[k]);
				}
				if (w > wmax) {
					wmax = w;
					winner = j;
				}
			}
		}

		chosen[winner] = 1;
		group->tasks[i] = winner;
		group->ntasks++;
		group->load += loads[winner];
/*		group->elements[i] = &groups[level-1][winner];*/
	}
}

static void network_generate_last_group (comm_matrix_t *m, uint32_t ntasks, char *chosen, machine_task_group_t *group, double *loads)
{
	uint32_t i, j;
	
	group->ntasks = 0;
	group->load = 0.0;
	i = 0;
	
	for (j=0; j<ntasks; j++) {
		if (unlikely(!chosen[j])) {
			chosen[j] = 1;
			
			group->tasks[i] = j;
			group->ntasks++;
			group->load += loads[j];
			
			i++;
		}
	}
}

void network_generate_groups_load (comm_matrix_t *m, uint32_t ntasks, machine_task_group_t *groups, uint32_t nmachines, double *loads)
{
	uint32_t i, total_pus, done_pus;
	double avg_load_per_pu, total_load, done_load, total_group_load;
	
	total_load = 0.0;
	for (i=0; i<ntasks; i++) {
		total_load += loads[i];
		chosen[i] = 0;
	}

	total_pus = 0;
	for (i=0; i<nmachines; i++) {
		groups[i].ntasks = 0;
		total_pus += groups[i].npus;
	}

	done_load = 0.0;
	done_pus = 0;
	
	for (i=0; i<(nmachines-1); i++) {
		avg_load_per_pu = (double)(total_load - done_load) / (double)(total_pus - done_pus);
		total_group_load = avg_load_per_pu * (double)groups[i].npus;
		network_generate_group_lb(m, ntasks, chosen, &groups[i], loads, total_group_load);

		done_load += groups[i].load;
		done_pus += groups[i].npus;
	}
	network_generate_last_group(m, ntasks, chosen, &groups[nmachines-1], loads);
	
	network_create_comm_matrices(m, groups, nmachines);
}

