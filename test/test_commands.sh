#!/bin/bash

set -e

# Basic corruption
yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -target corruption"

# Basic pairwise
yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -target pairwise"

# Basic hybrid
yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -target hybrid"

# Set key percent and key
yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -key-percent 5 -key 0a239e"

# Set key bits and key
yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -key-percent 5 -key 0a239e"

# Explore
yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -explore -output-dir logs"

# No analysis
yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -nb-analysis-keys 0"

# Change port name
yosys -m moosic -p "read_blif benchmarks/blif/c1355.blif; flatten; synth; logic_locking -port-name test_port"
