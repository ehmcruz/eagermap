#!/bin/bash

echo "machines:"

logical="1"
for core in `seq 0 1 56`
do
	echo -n "core$core 4 "
	
	for l in `seq 0 1 3`
	do
		if [ "$logical" == "228" ]; then
			logical="0"
		fi
		echo -n "$logical"
		if [ "$l" != "3" ]; then
			echo -n ","
		fi
		logical=$(($logical + 1))
	done
	
	echo ""
done

echo "links:"
