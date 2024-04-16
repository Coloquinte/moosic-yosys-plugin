#!/bin/bash

cd benchmarks
mkdir -p run_all scripts

key=$(printf "123456789abcdef%.0s" {1..100})

function run_benchmark() {
	benchmark=$1
	antisat=$2
	name=$(basename "${benchmark}" .blif)
	log_file="run_all/${name}_${antisat}.log"
	script_file="scripts/run_all_${name}.ys"
	echo "Running benchmark ${name} with sat countermeasure ${antisat}"

	echo "# Original design" >"${script_file}"
	echo "read_blif -sop ${benchmark}" >>"${script_file}"
	echo "flatten; synth" >>"${script_file}"
	echo "rename -top original; design -stash original" >>"${script_file}"

	echo "# Unlocked design" >>"${script_file}"
	echo "read_blif -sop ${benchmark}" >>"${script_file}"
	echo "flatten; synth" >>"${script_file}"
	echo "logic_locking -target outputs -nb-antisat 16 -antisat ${antisat} -key ${key}" >>"${script_file}"
	echo "synth; check -assert" >>"${script_file}"
	echo "ll_unlock -key ${key}; synth; check -assert" >>"${script_file}"
	echo "rename -top unlocked; design -stash unlocked" >>"${script_file}"

	echo "# Equivalence checking" >>"${script_file}"
	echo "design -copy-from original -as original original" >>"${script_file}"
	echo "design -copy-from unlocked -as unlocked unlocked" >>"${script_file}"
	echo "equiv_make original unlocked equiv" >>"${script_file}"
	echo "equiv_simple; equiv_struct; equiv_status" >>"${script_file}"
	cmd="yosys -m moosic -s ${script_file} > ${log_file}"
	eval "$cmd" && { echo "Finished ${name}"; } || {
		echo "Failure on ${name}: ${cmd}"
		exit 1
	}
}

for benchmark in blif/iscas85*.blif blif/iscas89*.blif blif/iscas99*.blif blif/lgsynth91*.blif blif/epfl*.blif blif/mcnc*.blif; do
	for antisat in none antisat caslock sarlock skglock; do
		run_benchmark "${benchmark}" "${antisat}"
	done
done
wait
