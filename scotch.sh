#!/bin/bash

set -o errexit -o pipefail -o posix -o nounset

# directory of this script
DIR="$( cd -P "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ $# -lt 2 -o $# -gt 3 ]; then
	echo "Usage: $0 <affinity.csv> <topology.txt> [load.txt]"
	exit 1
fi

if [ hash scotch_amk_grf 2>/dev/null ]; then
	amkgrf=scotch_amk_grf
else
	amkgrf=amk_grf
fi

affinity=$1
topology=$2

if [ $# -eq 3 ]; then
	load=$3
else
	load=""
fi

./eagermap $affinity $topology $load -norm -mode pscotch
$amkgrf scotch-topology.grf scotch-topology.tgt
scotch_gmap scotch-comm-matrix.grf scotch-topology.tgt > scotch-map.txt
./eagermap $affinity $topology $load -norm -mode mscotch scotch-map.txt

rm -f scotch-map.txt
