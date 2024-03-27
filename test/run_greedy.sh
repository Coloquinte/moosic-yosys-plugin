#!/bin/bash

cd benchmarks

function run_benchmark () {
	benchmark=$1
	percent=$2
	target=$3
	yosys -m moosic -p "read_blif -sop ${benchmark}; flatten; synth; logic_locking -nb-locked ${percent}% -target ${target}" && { echo "Finished ${name}"; } || { echo "Failure on ${name}: ${cmd}"; exit 1; }
}

echo "benchmark,key_percent,moosic,fll,kip" > corruptibility.csv
for benchmark in blif/iscas85*.blif blif/iscas89*.blif blif/iscas99*.blif blif/lgsynth91*.blif blif/epfl*.blif blif/mcnc*.blif
do
	for percent in 1 2 4 8 16
	do
        	name=$(basename "${benchmark}" .blif)
		echo -n "${name},${percent}" >> corruptibility.csv
		for target in corruption fll kip
		do
			echo "Running benchmark ${name} with ${percent}% key and target ${target}"
			echo -n "," >> corruptibility.csv
			run_benchmark "${benchmark}" "${percent}" "${target}" | grep "corruption result" -A 1 | tail -n 1 | sed "s/\s*\(.*\)% corruption.*/\1/" | tr -d '\n' >> corruptibility.csv
		done
		echo >> corruptibility.csv
	done
done
