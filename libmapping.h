#ifndef __LIBMAPPING_H__
#define __LIBMAPPING_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <wctype.h>
#include <inttypes.h>
#include <assert.h>
#include <ctype.h>

#define MAX_THREADS_BITS 14
#define MAX_THREADS (1 << MAX_THREADS_BITS)

#include "lib.h"
#include "graph.h"
#include "topology.h"
#include "mapping-algorithms.h"

#endif
