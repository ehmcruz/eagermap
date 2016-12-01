#include "libmapping.h"

static char chosen[MAX_THREADS];

static void balance_load (machine_task_group_t *gh, machine_task_group_t *gl, double *loads)
{
	int i, best_diff_i;
	double diff, best_diff, target_diff;
	
	if (gh->load < gl->load) {
		machine_task_group_t *tmp;
		tmp = gh;
		gh = gl;
		gl = tmp;
	}
	
/*	target_diff = (gh->load - gl->load) / 2.0;*/
/*	*/
/*	best_diff_i = 0;*/
/*	best_diff = gh->load - loads[ group->tasks[0] ] - target_diff;*/
/*	*/
/*	for (i=1; i<gh->nelements; i++) {*/
/*		diff = gh->load - loads[ group->tasks[i] ] - target_diff;*/
/*	}*/
}

static void balance_load_remove_free_pu (machine_task_group_t *groups, double *loads, uint32_t nmachines)
{
	int i, taskh, h;
	machine_task_group_t *gl, *gh;
	
	do {
		gl = NULL;
		
		for (i=0; i<nmachines; i++) {
			if (groups[i].ntasks < groups[i].npus) {
				gl = &groups[i];
				break;
			}
		}
		
		if (gl == NULL)
			return;
		
		gh = NULL;
	
		for (i=0; i<nmachines; i++) {
			if (groups[i].ntasks > groups[i].npus && (gh == NULL || groups[i].load > gh->load))
				gh = &groups[i];
		}
		
		if (gh == NULL)
			return;
		
		taskh = -1;
		for (i=0; i<gh->ntasks; i++) {
			if (taskh == -1 || loads[ gh->tasks[i] ] < loads[ taskh ]) {
				taskh = gh->tasks[i];
				h = i;
			}
		}
		
		if (taskh != -1) {
			printf("sent task %i(%.3f) to group %i\n", taskh, loads[taskh], gl->id);
	
			gh->tasks[h] = gh->tasks[ gh->ntasks-1 ];
			gh->load -= loads[taskh];
			gh->ntasks--;
		
			gl->tasks[ gl->ntasks ] = taskh;
			gl->load += loads[taskh];
			gl->ntasks++;
		}
	} while (gl != NULL);
}

static int balance_load_high_to_low (machine_task_group_t *groups, double *loads, uint32_t nmachines)
{
	int i, taskh, h;
	machine_task_group_t *gh, *gl;
	
	gh = &groups[0];
	gl = &groups[0];
	
	for (i=1; i<nmachines; i++) {
		if (groups[i].load > gh->load)
			gh = &groups[i];
		if (groups[i].load < gl->load)
			gl = &groups[i];
	}
	
	if (gh == gl)
		return 0;
	
	if (gh->ntasks < 2)
		return 0;
	
	taskh = -1;
	for (i=0; i<gh->ntasks; i++) {
		if (taskh == -1 || loads[ gh->tasks[i] ] < loads[ taskh ]) {
			taskh = gh->tasks[i];
			h = i;
		}
	}
	
	if (taskh != -1) {
		printf("sent task %i(%.3f) to group %i\n", taskh, loads[taskh], gl->id);
	
		gh->tasks[h] = gh->tasks[ gh->ntasks-1 ];
		gh->load -= loads[taskh];
		gh->ntasks--;
		
		gl->tasks[ gl->ntasks ] = taskh;
		gl->load += loads[taskh];
		gl->ntasks++;
		
		return 1;
	}

	
	return 0;
}

static int balance_load_exchange (machine_task_group_t *gh, machine_task_group_t *gl, double *loads)
{
	int i, h, l, highest, taskh, taskl;
	
	if (gh->ntasks == 0 || gl->ntasks == 0)
		return 0;
	
	if (gh->load < gl->load) {
		machine_task_group_t *tmp;
		tmp = gh;
		gh = gl;
		gl = tmp;
	}
	
	h = 0;
	taskh = gh->tasks[0];
	for (i=1; i<gh->ntasks; i++) {
		if (loads[ gh->tasks[i] ] > loads[ taskh ]) {
			taskh = gh->tasks[i];
			h = i;
		}
	}
	
	taskl = -1;
	for (i=0; i<gl->ntasks; i++) {
		if (loads[ gl->tasks[i] ] < loads[taskh] && (taskl == -1 || loads[ gl->tasks[i] ] > loads[ taskl ])) {
			taskl = gl->tasks[i];
			l = i;
		}
	}
	
	if (taskl != -1) {
		printf("exchanged tasks %i(%.3f) and %i(%.3f)\n", taskh, loads[taskh], taskl, loads[taskl]);
	
		gh->tasks[h] = taskl;
		gh->load -= loads[taskh];
		gh->load += loads[taskl];
		
		gl->tasks[l] = taskh;
		gl->load += loads[taskh];
		gl->load -= loads[taskl];
		
		return 1;
	}
	
	return 0;
}

/*
	2       1      target_diff = 0.5
	0.2     0.8    diff = 0.6
	
*/

static void network_generate_group_lb (comm_matrix_t *m, uint32_t ntasks, char *chosen, machine_task_group_t *group, double *loads, double max_load)
{
	weight_t w, wmax;
	uint32_t winner = 0;
	uint32_t i, j, k;

	group->ntasks = 0;
	group->load = 0.0;

/*printf("max_load %.3f", max_load);*/
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
		
/*		if ((group->load + loads[winner]) > (max_load*1.12))*/
/*			break;*/

		chosen[winner] = 1;
		group->tasks[i] = winner;
		group->ntasks++;
		group->load += loads[winner];
/*printf(" load(%i) %.3f", winner, loads[winner]);*/
/*		group->elements[i] = &groups[level-1][winner];*/
	}
/*printf(" total %.3f\n", group->load);*/
//getchar();
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
	uint32_t total_pus, done_pus;
	int i, j;
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

#if 0
/*	for (k=0; k<5; k++) {*/
		for (i=0, j=nmachines-1; i<j; i++, j--)
			balance_load_exchange(&groups[i], &groups[j], loads);
/*	}*/
	
	for (i=0; i<nmachines*nmachines; i++)
		balance_load_high_to_low(groups, loads, nmachines);
		
	if (ntasks > nmachines)
		balance_load_remove_free_pu(groups, loads, nmachines);
#endif

	network_create_comm_matrices(m, groups, nmachines);
}

