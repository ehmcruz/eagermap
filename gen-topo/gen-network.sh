#!/bin/bash

user="ehmcruz"

machines="$*"

cmd=`tail -n +2 gen-topo.sh`

echo "machines:"

for m in $machines
do
	echo -n "$m "
	ssh $user@$m "$cmd"
done
