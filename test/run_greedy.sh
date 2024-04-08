#!/bin/bash

cd benchmarks

function run_benchmark () {
	benchmark=$1
	percent=$2
	target=$3
	yosys -m moosic -p "read_blif -sop ${benchmark}; flatten; synth; logic_locking -nb-locked ${percent}% -target ${target}" && { echo "Finished ${name}"; } || { echo "Failure on ${name}: ${cmd}"; exit 1; }
}

for target in corruption fll kip
do
	echo "benchmark key_percent corruptibility" > "corruptibility_${target}.csv"
done

for benchmark in blif/iscas85*.blif blif/iscas89*.blif blif/iscas99*.blif blif/lgsynth91*.blif blif/epfl*.blif blif/mcnc*.blif
do
	for percent in 1 2 4 8 16
	do
        	name=$(basename "${benchmark}" .blif)
		for target in corruption fll kip
		do
			echo -n "${name} ${percent} " >> "corruptibility_${target}.csv"
			echo "Running benchmark ${name} with ${percent}% key and target ${target}"
			run_benchmark "${benchmark}" "${percent}" "${target}" | grep "corruption result" -A 1 | tail -n 1 | sed "s/\s*\(.*\)% corruption.*/\1/" | tr -d '\n' >> "corruptibility_${target}.csv"
			echo >> "corruptibility_${target}.csv"
		done
	done
done

for target in corruption fll kip
do
	sort -nk3 "corruptibility_${target}.csv" -o "corruptibility_${target}.csv"
done
