#!/bin/bash

cat benchmarks/estimate/* | sort -ur > estimate.csv
mkdir -p figures
gnuplot -s report/build_figures.gnuplot

