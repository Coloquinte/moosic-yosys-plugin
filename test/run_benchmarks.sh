#!/bin/bash

dirs="logs scripts"
targets="pairwise corruption"
key_percents="1 2 5 10"

cd benchmarks

for dir in $dirs
do
	for target in $targets
	do
		for key_percent in $key_percents
		do
			mkdir -p "${dir}/${target}/key_${key_percent}pc"
		done
	done
done

for benchmark in blif/c*.blif
do
        name=$(basename "${benchmark}" .blif)
	echo "Running benchmark ${name}"
        for target in $targets
        do
		echo "    Target ${target}"
		for key_percent in $key_percents
		do
			echo "        Key ${key_percent}%"
			log_file="logs/${target}/key_${key_percent}pc/${name}.log"
			script_file="scripts/${target}/key_${key_percent}pc/${name}.ys"
			echo "read_blif ${benchmark}" > "${script_file}"
			echo "flatten; synth" >> "${script_file}"
			echo "logic_locking -key-percent ${key_percent} -target ${target}" >> "${script_file}"
			cmd="yosys -m moosic-yosys-plugin -s ${script_file} > ${log_file}"
			eval "$cmd" || { echo "Failure on ${name} target ${target} key ${key_percent}%: ${cmd}"; exit 1; }
		done
	done
done

cd ..
tar -c benchmarks/scripts/ benchmarks/logs | xz -9 - > benchmark_results.tar.xz
