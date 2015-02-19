#ifndef __LIBMAPPING_H__
#define __LIBMAPPING_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#define MAX_THREADS_BITS 10
#define MAX_THREADS (1 << MAX_THREADS_BITS)

#include "lib.h"
#include "graph.h"
#include "topology.h"
#include "mapping-algorithms.h"
#include "RubyConfig.h"

#endif
