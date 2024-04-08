#!/bin/bash

# This script is run after having run the test scripts to build all figures in the reports

cat benchmarks/estimate/iscas* | sort -ur > estimate.csv
mkdir -p figures figures/area figures/delay figures/estimate figures/area_pairwise figures/delay_pairwise
gnuplot -s report/build_figures.gnuplot

