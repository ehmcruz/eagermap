#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <hwloc.h>

#define dprintf(...) printf(__VA_ARGS__)

static void lm_hwloc_load_topology_arities (hwloc_topology_t hwloc_topology, hwloc_obj_t obj, int level, FILE *fp, int printed)
{
	uint32_t i;
	
	if (obj->arity == 1) {
		lm_hwloc_load_topology_arities(hwloc_topology, obj->children[0], level, fp, printed);
		return;
	}

	if (obj->arity) {
		if (printed)
			fprintf(fp, ",");
		printed = 1;
		fprintf(fp, "%i", (int)obj->arity);
		dprintf("level %i - %i\n", level, (int)obj->arity);
	}

	if (!obj->arity) {
		dprintf("found core\n");
	}
	
	if (obj->arity) {
		lm_hwloc_load_topology_arities(hwloc_topology, obj->children[0], level+1, fp, printed);
	}
}

static void load_hwloc (FILE *fp)
{
	hwloc_topology_t hwloc_topology;

	hwloc_topology_init(&hwloc_topology);
/*		hwloc_topology_ignore_all_keep_structure(&hwloc_topology);*/
	hwloc_topology_load(hwloc_topology);

	lm_hwloc_load_topology_arities(hwloc_topology, hwloc_get_root_obj(hwloc_topology), 0, fp, 0);
}

int main (int argc, char **argv)
{
	FILE *fp;
	
	fp = fopen("topo.txt", "w");
	assert(fp != NULL);
	load_hwloc(fp);
	fclose(fp);
	
	return 0;
}
