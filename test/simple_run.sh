#!/bin/bash

target=corruption
key_percent=5
benchmark=benchmarks/blif/c1355.blif

yosys -m moosic -p "read_blif ${benchmark}; flatten; synth; logic_locking -key-percent ${key_percent} -target ${target}"
