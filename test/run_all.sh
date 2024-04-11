#!/bin/bash

set -e

cd benchmarks
mkdir -p run_all

function run_benchmark() {
	benchmark=$1
	antisat=$2
	yosys -m moosic -p "read_blif -sop ${benchmark}; flatten; synth; logic_locking -target outputs -nb-antisat 16 -antisat ${antisat}; synth; check -assert"
}

for benchmark in blif/iscas85*.blif blif/iscas89*.blif blif/iscas99*.blif blif/lgsynth91*.blif blif/epfl*.blif blif/mcnc*.blif; do
	for antisat in none antisat caslock sarlock skglock; do
		name=$(basename "${benchmark}" .blif)
		echo "Running benchmark ${name} with sat countermeasure ${antisat}"
		run_benchmark "${benchmark}" "${antisat}" >"run_all/${name}_${antisat}.log"
	done
done
wait
