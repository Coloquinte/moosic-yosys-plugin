#!/bin/bash

for benchmark in benchmarks/blif/*.blif
do
        name=$(basename "${benchmark}" .blif)
	echo "Running benchmark ${name}"
        for target in pairwise corruption
        do
                yosys -m moosic-yosys-plugin  -p "read_blif ${benchmark}; flatten; synth; logic_locking -key-percent ${key_percent} -target ${target}" > /dev/null \
                        || { echo "Failure on ${name} target ${target}"; exit 1; }
        done
done
