#!/bin/bash

dirs="logs scripts estimate full area delay area_approx delay_approx area_corr delay_corr"

cd benchmarks

for dir in $dirs
do
	mkdir -p "${dir}"
done

function run_benchmark () {
	benchmark=$1
        name=$(basename "${benchmark}" .blif)
	echo "Running benchmark ${name}"
	log_file="logs/${name}.log"
	script_file="scripts/${name}.ys"
	echo "read_blif ${benchmark}" > "${script_file}"
	echo "flatten; synth" >> "${script_file}"
	# Full Pareto front
	echo "ll_explore -area -delay -corruptibility -test-corruptibility -output-corruptibility -iter-limit ${iter_limit} -time-limit ${time_limit} -output full/${name}.csv" >> "${script_file}"
	# Comparison between estimates
	echo "ll_explore -area -corruptibility -compare-estimate -iter-limit ${iter_limit} -time-limit ${time_limit} -output estimate/${name}.csv" >> "${script_file}"
	# Area/corruptibility
	echo "ll_explore -area -corruptibility -no-estimate -iter-limit ${iter_limit} -time-limit ${time_limit} -output area_approx/${name}.csv" >> "${script_file}"
	echo "ll_explore -area -corruptibility -iter-limit ${iter_limit} -time-limit ${time_limit} -output area/${name}.csv" >> "${script_file}"
	echo "ll_explore -area -corruption -iter-limit ${iter_limit} -time-limit ${time_limit} -output area_corr/${name}.csv" >> "${script_file}"
	# Delay/corruptibility
	echo "ll_explore -delay -corruptibility -no-estimate -iter-limit ${iter_limit} -time-limit ${time_limit} -output delay_approx/${name}.csv" >> "${script_file}"
	echo "ll_explore -delay -corruptibility -iter-limit ${iter_limit} -time-limit ${time_limit} -output delay/${name}.csv" >> "${script_file}"
	echo "ll_explore -delay -corruption -iter-limit ${iter_limit} -time-limit ${time_limit} -output delay_corr/${name}.csv" >> "${script_file}"
	cmd="yosys -m moosic -s ${script_file} > ${log_file}"
	eval "$cmd" || { echo "Failure on ${name}: ${cmd}"; exit 1; }
}

if [ "$1" = "-all" ]
then
	time_limit=600
	iter_limit=1000000
	echo "Executing full benchmark set"
	for benchmark in blif/*.blif
	do
		run_benchmark $benchmark
	done
else
	time_limit=20
	iter_limit=10000
	echo "Executing small benchmark set"
	for benchmark in blif/iscas85*.blif
	do
		run_benchmark $benchmark
	done
fi

cd ..
tar -c benchmarks/estimate benchmarks/area benchmarks/delay benchmarks/area_approx benchmarks/delay_approx benchmarks/full | xz -9 - > benchmark_results.tar.xz
