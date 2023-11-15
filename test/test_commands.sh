#!/bin/bash

set -e

cmd=""
if [ "$ENABLE_VALGRIND" == "1" ]
then
	cmd="valgrind --error-exitcode=11 "
fi

# Basic corruption
$cmd yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -target corruption"

# Basic pairwise
$cmd yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -target pairwise"

# Basic hybrid
$cmd yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -target hybrid"

# Basic pairwise without deduplication
$cmd yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -target pairwise-no-dedup"

# Set key percent and key
$cmd yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -key-percent 5 -key 0a239e"

# Set key bits and key
$cmd yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -key-bits 10 -key 0a239e"

# Set number of test vectors and keys
$cmd yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -nb-test-vectors 99 -nb-analysis-keys 95 -nb-analysis-vectors 1079"

# No analysis
$cmd yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -nb-analysis-keys 0"

# Change port name
$cmd yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -port-name test_port"
