#!/bin/bash

cat benchmarks/estimate/iscas* | sort -ur > estimate.csv
mkdir -p figures
gnuplot -s report/build_figures.gnuplot

