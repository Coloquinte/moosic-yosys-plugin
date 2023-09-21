#!/bin/bash

for benchmark in $(find iscas/ISCAS* -name *.v)
do
        name=$(basename "${benchmark}" .v)
	echo "Running benchmark ${name}"
        for target in pairwise corruption
        do
                yosys -m moosic-yosys-plugin  -p "read_verilog ${benchmark}; flatten; synth; select -module ${name}; logic_locking -key-percent ${key_percent} -target ${target}" > /dev/null \
                        || { echo "Failure on ${benchmark} target ${target}"; exit 1; }
        done
done
