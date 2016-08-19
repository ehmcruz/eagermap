#!/bin/bash

string=$(hwloc-ls -p --of console --merge --no-io -s | sed 's/.*:[^0-9]*\([0-9]*\) .*/\1/')
#echo "$string"
readarray -t lines <<<"$string"

#for line in "${lines[@]}"
#do
#	echo "--> $line"
#done

#echo ${#lines[@]}
max=$((${#lines[@]} - 1))
#echo $max

#echo -e "${lines[1]}\n\n"

#for i in `seq 1 1 $max`
#do
#	echo "--> ${lines[$i]}"
#done

#echo -n "arities:"

bef=${lines[0]}
for i in `seq 1 1 $max`
do
	arity=$((${lines[$i]} / $bef))
	echo -n "$arity"
	
	if [ $i -ne $max ]; then
		echo -n ","
	fi
	
	bef=${lines[$i]}
done

PULIST=$(hwloc-ls -p --no-io --no-caches --merge --of console | grep PU | sed s,[A-Z#\ ],,g | tr '\n    ' ',' | sed s,.$,,)

echo " $PULIST"
