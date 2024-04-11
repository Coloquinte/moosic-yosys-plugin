#!/bin/bash

cd benchmarks
mkdir -p sat_attack

benchmarks="blif/iscas85*.blif blif/iscas89*.blif blif/iscas99*.blif blif/lgsynth91*.blif blif/epfl*.blif blif/mcnc*.blif"
key=$(printf "7%.0s" {1..500})
time_limit=120
batch=2
thresholds="0 0.1"
countermeasures="antisat caslock sarlock skglock+"
nb_antisats="16 24 32"

function run_benchmark () {
	benchmark=$1
	error_threshold=$2
	nb_antisat=$3
	antisat=$4
	name=$(basename "${benchmark}" .blif)
	stdbuf -oL yosys -m moosic -p "read_blif -sop ${benchmark}; flatten; synth; logic_locking -target outputs -nb-antisat ${nb_antisat} -antisat ${antisat} -key ${key}; synth; ll_sat_attack -key ${key} -time-limit ${time_limit} -error-threshold ${error_threshold}" > "sat_attack/${name}_${antisat}_${nb_antisat}_${error_threshold}.log"
}

i=0
for benchmark in $benchmarks
do
	for error_threshold in $thresholds
	do
		for antisat in $countermeasures
		do
			for nb_antisat in $nb_antisats
			do
				((i=i%batch)); ((i++==0)) && wait
				name=$(basename "${benchmark}" .blif)
				echo "Running benchmark ${name} with sat countermeasure ${antisat} (${nb_antisat}), threshold ${error_threshold}"
				run_benchmark "${benchmark}" "${error_threshold}" "${nb_antisat}" "${antisat}" &
			done
		done
	done
done
wait
