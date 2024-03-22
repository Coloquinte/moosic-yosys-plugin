#!/bin/bash

dirs="sat_attack"

cd benchmarks

for dir in $dirs
do
	mkdir -p "${dir}"
done

function run_benchmark () {
	benchmark=$1
        name=$(basename "${benchmark}" .blif)
	echo "Running benchmark ${name}"
	log_file="sat_attack/${name}.log"
	script_file="scripts/${name}.ys"
	echo "read_blif -sop ${benchmark}" > "${script_file}"
	echo "flatten; synth" >> "${script_file}"
	# Apply the locking
	echo "logic_locking -key-bits 64 -key 555555555555555555555555555555555555555" >> "${script_file}"
	# Sat attack
	echo "ll_sat_attack -time-limit 10 -key 555555555555555555555555555555555555555" >> "${script_file}"
	cmd="timeout 600 yosys -m moosic -s ${script_file} > ${log_file}"
	eval "$cmd" && { echo "Finished ${name}"; } || { echo "Failure on ${name}: ${cmd}"; }
}

if [ "$1" = "-all" ]
then
	echo "Executing full benchmark set"
	for benchmark in blif/iscas85*.blif blif/iscas89*.blif blif/iscas99*.blif blif/lgsynth*.blif blif/mcnc*.blif blif/epfl*.blif
	do
		run_benchmark "${benchmark}"
	done
	wait
elif [ "$1" != "" ]
then
	echo "Invalid argument; only -all is accepted"
	exit 1
else
	echo "Executing small benchmark set"
	for benchmark in blif/iscas85*.blif
	do
		run_benchmark "${benchmark}"
	done
fi

cd ..
tar -c benchmarks/sat_attack | xz -9 - > sat_attack.tar.xz
