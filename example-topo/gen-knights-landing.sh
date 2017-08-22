#!/bin/bash

echo "machines:"

for core in `seq 0 1 67`
do
	echo -n "core$core 4 "
	
	for l in 0 68 136 204
	do
		echo -n "$((core + $l)) "
	done
	
	echo ""
done

echo "links:"
