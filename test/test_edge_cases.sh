#!/bin/bash
#

set -e

cmd=""
if [ "$ENABLE_VALGRIND" == "1" ]
then
	cmd="valgrind --error-exitcode=11 "
fi

for nb_locked in 0 16 1024
do
	for nb_antisat in 0 16 1024
	do
		for antisat in none antisat sarlock skglock
		do
			$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -nb-locked ${nb_locked} -nb-antisat ${nb_antisat} -antisat ${antisat}"
		done
	done
done
