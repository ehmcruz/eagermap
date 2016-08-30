#include "libmapping.h"

static char chosen[MAX_THREADS];

uint32_t **network_distance_machines;

#define dist(i,j) network_distance_machines[ (i) ][ (j) ]
#define MAX_DIST ((UINT_MAX-10)/2)

void network_floyd_warshall (machine_t *machines, uint32_t nmachines)
{
	uint32_t k, i, j;
	
	printf("UINT_MAX is %u\nMAX_DIST is %u\n", UINT_MAX, MAX_DIST);
	
	network_distance_machines = libmapping_matrix_malloc(nmachines, nmachines, sizeof(uint32_t));
	
	for (i=0; i<nmachines; i++) {
		for (j=0; j<nmachines; j++)
			network_distance_machines[i][j] = MAX_DIST;
	}
	
	for (i=0; i<nmachines; i++) {
		dist(i,i) = 0;
		
		for (j=0; j<machines[i].nlinks; j++)
			dist(i, machines[i].links[j].machine->id ) = machines[i].links[j].weight;
	}
	
	for (k=0; k<nmachines; k++) {
		for (i=0; i<nmachines; i++) {
			for (j=0; j<nmachines; j++) {
				if (dist(i,j) > dist(i,k) + dist(k,j))
					dist(i,j) = dist(i,k) + dist(k,j);
			}
		}
	}
	
	printf("network distance matrix from floyd warshall\n");
	for (i=0; i<nmachines; i++) {
		for (j=0; j<nmachines; j++)
			printf("%i,", (network_distance_machines[i][j] != MAX_DIST) ? network_distance_machines[i][j] : -1);
		printf("\n");
	}
	printf("\n");
}

static void network_generate_group (comm_matrix_t *m, uint32_t ntasks, char *chosen, machine_task_group_t *group)
{
	weight_t w, wmax;
	uint32_t winner = 0;
	uint32_t i, j, k;

	for (i=0; i<group->ntasks; i++) { // in each iteration, find one element of the group
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
/*		group->elements[i] = &groups[level-1][winner];*/
	}
}

void network_create_comm_matrices (comm_matrix_t *m, machine_task_group_t *groups, uint32_t nmachines)
{
	uint32_t i, j, k;
	for (i=0; i<nmachines; i++) {
		libmapping_comm_matrix_init(&groups[i].cm, groups[i].ntasks);
		
		for (j=0; j<groups[i].ntasks; j++) {
			for (k=0; k<groups[i].ntasks; k++) {
				comm_matrix_write(groups[i].cm, j, k, comm_matrix_ptr_el(m, groups[i].tasks[j], groups[i].tasks[k]));
			}
		}
	}
}

void network_generate_groups (comm_matrix_t *m, uint32_t ntasks, machine_task_group_t *groups, uint32_t nmachines)
{
	uint32_t done, i, total_pus, done_pus;
	double avg_tasks_per_pu;
	
	total_pus = 0;
	for (i=0; i<nmachines; i++) {
		groups[i].ntasks = 0;
		total_pus += groups[i].npus;
	}

	done = 0;
	done_pus = 0;
	for (i=0; i<(nmachines-1); i++) {
		avg_tasks_per_pu = (double)(ntasks - done) / (double)(total_pus - done_pus);
		groups[i].ntasks = avg_tasks_per_pu * (double)groups[i].npus;
		done += groups[i].ntasks;
		done_pus += groups[i].npus;
	}
	groups[ nmachines-1 ].ntasks = ntasks - done;

	for (i=0; i<ntasks; i++)
		chosen[i] = 0;
	
	for (i=0; i<nmachines; i++)
		network_generate_group(m, ntasks, chosen, &groups[i]);
	
	network_create_comm_matrices(m, groups, nmachines);
}

void network_map_groups_to_machines (machine_task_group_t *groups, machine_t *machines, uint32_t nmachines)
{
	uint32_t i;
	
	for (i=0; i<nmachines; i++) {
		machines[i].cm = &groups[i].cm;
		machines[i].ntasks = groups[i].ntasks;
		machines[i].load = groups[i].load;
		machines[i].tasks = groups[i].tasks;
	}
}

