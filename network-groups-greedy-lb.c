#include "libmapping.h"

static char chosen[MAX_THREADS];

static void network_generate_group_lb (comm_matrix_t *m, uint32_t ntasks, char *chosen, machine_t *machine, double *loads, double max_load)
{
	weight_t w, wmax;
	uint32_t winner = 0;
	uint32_t i, j, k;

	machine->ntasks = 0;
	machine->load = 0.0;
	
	for (i=0; machine->load<max_load; i++) { // in each iteration, find one element of the group
		wmax = -1;
		for (j=0; j<ntasks; j++) { // iterate over all elements to find the one that maximizes the communication relative to the elements that are already in the group
			if (!chosen[j]) {
				w = 0;
				for (k=0; k<i; k++) {
					w += comm_matrix_ptr_el(m, j, machine->tasks[k]);
				}
				if (w > wmax) {
					wmax = w;
					winner = j;
				}
			}
		}

		chosen[winner] = 1;
		machine->tasks[i] = winner;
		machine->ntasks++;
		machine->load += loads[winner];
/*		group->elements[i] = &groups[level-1][winner];*/
	}
}

static void network_generate_last_group (comm_matrix_t *m, uint32_t ntasks, char *chosen, machine_t *machine, double *loads)
{
	uint32_t i, j;
	
	machine->ntasks = 0;
	machine->load = 0.0;
	i = 0;
	
	for (j=0; j<ntasks; j++) {
		if (unlikely(!chosen[j])) {
			chosen[j] = 0;
			
			machine->tasks[i] = j;
			machine->ntasks++;
			machine->load += loads[j];
			
			i++;
		}
	}
}

static void network_create_comm_matrices (comm_matrix_t *m, machine_t *machines, uint32_t nmachines)
{
	uint32_t i, j, k;
	for (i=0; i<nmachines; i++) {
		libmapping_comm_matrix_init(&machines[i].cm, machines[i].ntasks);
		
		for (j=0; j<machines[i].ntasks; j++) {
			for (k=0; k<machines[i].ntasks; k++) {
				comm_matrix_write(machines[i].cm, j, k, comm_matrix_ptr_el(m, machines[i].tasks[j], machines[i].tasks[k]));
			}
		}
	}
}

void network_generate_groups_load (comm_matrix_t *m, uint32_t ntasks, machine_t *machines, uint32_t nmachines, double *loads)
{
	uint32_t i, total_pus, done_pus;
	double avg_load_per_pu, total_load, done_load, total_machine_load;
	
	total_load = 0.0;
	for (i=0; i<ntasks; i++) {
		total_load += loads[i];
		chosen[i] = 0;
	}

	total_pus = 0;
	for (i=0; i<nmachines; i++) {
		machines[i].ntasks = 0;
		total_pus += machines[i].topology.pu_number;
	}

	done_load = 0.0;
	done_pus = 0;
	
	for (i=0; i<(nmachines-1); i++) {
		avg_load_per_pu = (double)(total_load - done_load) / (double)(total_pus - done_pus);
		total_machine_load = avg_load_per_pu * (double)machines[i].topology.pu_number;
		network_generate_group_lb(m, ntasks, chosen, &machines[i], loads, total_machine_load);

		done_load += machines[i].load;
		done_pus += machines[i].topology.pu_number;
	}
	network_generate_last_group(m, ntasks, chosen, &machines[nmachines-1], loads);
	
	network_create_comm_matrices(m, machines, nmachines);
}

