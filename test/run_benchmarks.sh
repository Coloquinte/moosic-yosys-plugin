#!/bin/bash

dirs="logs scripts results"
time_limit=600
iter_limit=1000000

cd benchmarks

for dir in $dirs
do
	mkdir -p "${dir}"
done

for benchmark in blif/c*.blif
do
        name=$(basename "${benchmark}" .blif)
	echo "Running benchmark ${name}"
	log_file="logs/${name}.log"
	script_file="scripts/${name}.ys"
	result_file="results/${name}.csv"
	echo "read_blif ${benchmark}" > "${script_file}"
	echo "flatten; synth" >> "${script_file}"
	echo "ll_explore -area -delay -corruptibility-estimate -iter-limit ${iter_limit} -time-limit ${time_limit} -output ${result_file}" >> "${script_file}"
	cmd="yosys -m moosic -s ${script_file} > ${log_file}"
	eval "$cmd" || { echo "Failure on ${name}: ${cmd}"; exit 1; }
done

cd ..
tar -c benchmarks/scripts/ benchmarks/logs benchmarks/results/ | xz -9 - > benchmark_results.tar.xz
