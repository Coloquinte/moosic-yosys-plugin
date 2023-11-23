
set terminal 'pngcairo' size 800,600

set datafile separator ','
list='b01 b02 b03 b04 b05 b06 b07 b08 b09 b10 b11 b12 b13 b14_1 b14 b15_1 b17_1 b17 c1196 c1238 c1355 c17 c1908 c2670 c3540 c432 c499 c5315 c6288 c7552 c880 s1196a s1196b s1196 s1238a s1238 s13207 s1423 s1488 s1494 s15850 s208a s208 s27a s27 s298 s344 s349 s35932 s382 s38417 s38584 s386a s386 s400 s420a s420 s444 s510 s526a s526 s5378 s641 s713 s820 s832 s838a s838 s9234 s953'

set xlabel font "Helvetica,24"
set ylabel font "Helvetica,24"
set xrange [0:100]
set yrange [0:100]
set key off

set xlabel 'Approximation (%)'
set ylabel 'Corruptibility (%)'

set output 'estimate.png'
plot 'estimate.csv' using 3:4 with points

do for [name in list] {
	set output 'estimate_'.name.'.png'
	plot 'benchmarks/estimate/'.name.'.csv' using 3:4 with points
}


set xlabel 'Area penalty (%)'
set ylabel 'Corruptibility (%)'

do for [name in list] {
	set output 'area_'.name.'.png'
	plot 'benchmarks/area/'.name.'.csv' using 2:3 with linespoints
}


set xlabel 'Delay penalty (%)'
set ylabel 'Corruptibility (%)'

do for [name in list] {
	set output 'delay_'.name.'.png'
	plot 'benchmarks/delay/'.name.'.csv' using 2:3 with linespoints
}
