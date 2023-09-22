#!/bin/bash

mkdir -p downloads
mkdir -p benchmarks
cd downloads

# ISCAS
mkdir -p iscas; cd iscas
wget -qN https://ddd.fit.cvut.cz/www/prj/Benchmarks/ISCAS.7z
7z x ISCAS.7z -aos
cd ..

# ITC99
mkdir -p itc99; cd itc99
wget -qN https://ddd.fit.cvut.cz/www/prj/Benchmarks/ITC99.7z
7z x ITC99.7z -aos
cd ..

# IWLS93
mkdir -p iwls93; cd iwls93
wget -qN https://ddd.fit.cvut.cz/www/prj/Benchmarks/IWLS93.7z
7z x IWLS93.7z -aos
cd ..

for dir in iscas/blif itc99/ITC99/Blif iwls93/blif
do
	family=$(echo $dir | cut -f 1 -d /)
	for file in $(ls "${dir}" | grep .blif)
	do
		cp "${dir}/${file}" "../benchmarks/${family}_${file}"
	done
done
cd ..

# Workaround for Yosys bug, where .end is mandatory while it should be optional
for file in benchmarks/*.blif
do
	grep .end "${file}" > /dev/null || { echo; echo .end; } >> "${file}"
done
