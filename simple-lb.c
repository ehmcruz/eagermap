#include "libmapping.h"

static machine_task_group_t* chose_group (machine_task_group_t *groups, uint32_t nmachines)
{
	int i;
	machine_task_group_t *group;
	
	group = groups;
	
	for (i=1; i<nmachines; i++) {
		if (groups[i].ntasks < group->ntasks)
			group = &groups[i];
		else if (groups[i].load < group->load)
			group = &groups[i];
	}
	
	return group;
}

void network_generate_groups_load_simple (comm_matrix_t *m, uint32_t ntasks, machine_task_group_t *groups, uint32_t nmachines, double *loads)
{
	int i;
	machine_task_group_t *group;
	
	for (i=0; i<nmachines; i++) {
		groups[i].ntasks = 0;
		groups[i].load = 0.0;
	}

	for (i=0; i<ntasks; i++) {
		group = chose_group(groups, nmachines);
		group->tasks[ group->ntasks ] = i;
		group->load += loads[i];
		group->ntasks++;
	}
	
	network_create_comm_matrices(m, groups, nmachines);
}
