#!/bin/bash

cat benchmarks/estimate/* | sort -ur > estimate.csv
gnuplot -s build_figures.gnuplot

