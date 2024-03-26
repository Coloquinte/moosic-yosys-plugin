#!/bin/bash

set -e

cmd=""
if [ "$ENABLE_VALGRIND" == "1" ]
then
	cmd="valgrind --error-exitcode=11 "
fi

# Basic corruption
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -target corruption"

# Basic pairwise
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -target pairwise"

# Basic hybrid
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -target hybrid"

# Basic FLL
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -target fault-analysis-fll"

# Basic KIP
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -target fault-analysis-kip"

# Lock outputs
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -target outputs"

# Basic pairwise without deduplication
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -target pairwise-no-dedup"

# Dry run option
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -dry-run"

# Set key percent and key
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -nb-locked 5% -key 0a239e"

# Set key bits and key
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -nb-locked 10 -key 0a239e"

# Set number of test vectors and keys
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -nb-test-vectors 99 -nb-analysis-keys 95 -nb-analysis-vectors 1079"

# No analysis
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -nb-analysis-keys 0"

# Change port name
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -port-name test_port"

# Run exploration
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; ll_explore -area -delay -corruptibility -iter-limit 1000 -time-limit 10 -nb-analysis-keys 121 -nb-analysis-vectors 67"

# All objectives at once
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; ll_explore -area -delay -corruptibility -test-corruptibility -output-corruptibility -iter-limit 1000 -time-limit 10"

# Show exploration result
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; ll_show -locking af53"

# Analyze exploration result
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; ll_analyze -locking af53 -nb-analysis-keys 121 -nb-analysis-vectors 67"

# Apply exploration result
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; ll_apply -locking af53 -port-name test_port"

# Sat attack
$cmd yosys -m moosic -p "read_blif benchmarks/blif/iscas85-c1355.blif; flatten; synth; logic_locking -key 555555; ll_sat_attack -key 555555"
