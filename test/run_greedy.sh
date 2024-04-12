#!/bin/bash

cd benchmarks
mkdir -p greedy

benchmarks="blif/iscas85*.blif blif/iscas89*.blif blif/iscas99*.blif blif/lgsynth91*.blif blif/epfl*.blif blif/mcnc*.blif"
targets="corruption fll kip"
percents="1 2 4 8 16"
batch=2

function run_benchmark() {
	benchmark=$1
	percent=$2
	target=$3
	name=$(basename "${benchmark}" .blif)
	yosys -m moosic -p "read_blif -sop ${benchmark}; flatten; synth; logic_locking -nb-locked ${percent}% -target ${target}"
}

for target in $targets; do
	echo "benchmark key_percent corruptibility" >"corruptibility_${target}.csv"
done

i=0
for benchmark in $benchmarks; do
	for percent in $percents; do
		name=$(basename "${benchmark}" .blif)
		for target in $targets; do
			((i = i % batch))
			((i++ == 0)) && wait
			echo "Running benchmark ${name} with ${percent}% key and target ${target}"
			run_benchmark "${benchmark}" "${percent}" "${target}" >"greedy/${name}_${target}_${percent}.log" &
		done
	done
done

for benchmark in $benchmarks; do
	for percent in $percents; do
		name=$(basename "${benchmark}" .blif)
		for target in $targets; do
			echo -n "${name} ${percent} " >>"corruptibility_${target}.csv"
			grep "corruption result" -A 1 "greedy/${name}_${target}_${percent}.log" | tail -n 1 | sed "s/\s*\(.*\)% corruption.*/\1/" | tr -d '\n' >>"corruptibility_${target}.csv"
			echo >>"corruptibility_${target}.csv"
		done
	done
done

for target in corruption fll kip; do
	LC_ALL=C sort -gk3 "corruptibility_${target}.csv" -o "corruptibility_${target}.csv"
done
