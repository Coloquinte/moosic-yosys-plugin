
set terminal 'pngcairo' size 800,600

set datafile separator ','
list='epfl-adder epfl-arbiter epfl-bar epfl-cavlc epfl-ctrl epfl-dec epfl-div epfl-hyp epfl-i2c epfl-int2float epfl-log2 epfl-max epfl-mem_ctrl epfl-multiplier epfl-priority epfl-router epfl-sin epfl-sqrt epfl-square epfl-voter iscas85-c1355 iscas85-c17 iscas85-c1908 iscas85-c2670 iscas85-c3540 iscas85-c432 iscas85-c499 iscas85-c5315 iscas85-c6288 iscas85-c7552 iscas85-c880 iscas89-c1196 iscas89-c1238 iscas89-s1196a iscas89-s1196b iscas89-s1196 iscas89-s1238a iscas89-s1238 iscas89-s13207 iscas89-s1423 iscas89-s1488 iscas89-s1494 iscas89-s15850 iscas89-s208a iscas89-s208 iscas89-s27a iscas89-s27 iscas89-s298 iscas89-s344 iscas89-s349 iscas89-s35932 iscas89-s382 iscas89-s38417 iscas89-s38584 iscas89-s386a iscas89-s386 iscas89-s400 iscas89-s420a iscas89-s420 iscas89-s444 iscas89-s510 iscas89-s526a iscas89-s526 iscas89-s5378 iscas89-s641 iscas89-s713 iscas89-s820 iscas89-s832 iscas89-s838a iscas89-s838 iscas89-s9234 iscas89-s953 iscas99-b01 iscas99-b02 iscas99-b03 iscas99-b04 iscas99-b05 iscas99-b06 iscas99-b07 iscas99-b08 iscas99-b09 iscas99-b10 iscas99-b11 iscas99-b12 iscas99-b13 iscas99-b14_1 iscas99-b14 iscas99-b15_1 iscas99-b17_1 iscas99-b17 iscas99-b18_1 iscas99-b18 iscas99-b19_1 iscas99-b19 iscas99-b20_1 iscas99-b20 iscas99-b21_1 iscas99-b21 iscas99-b22_1 iscas99-b22 lgsynth91-9symml lgsynth91-alu2 lgsynth91-alu4 lgsynth91-apex6 lgsynth91-apex7 lgsynth91-b1 lgsynth91-b9 lgsynth91-bigkey lgsynth91-C1355 lgsynth91-C17 lgsynth91-C1908 lgsynth91-C2670 lgsynth91-C3540 lgsynth91-C432 lgsynth91-C499 lgsynth91-C5315 lgsynth91-C6288 lgsynth91-C7552 lgsynth91-C880 lgsynth91-c8 lgsynth91-cc lgsynth91-cht lgsynth91-clma lgsynth91-clmb lgsynth91-cm138a lgsynth91-cm150a lgsynth91-cm151a lgsynth91-cm152a lgsynth91-cm162a lgsynth91-cm163a lgsynth91-cm42a lgsynth91-cm82a lgsynth91-cm85a lgsynth91-cmb lgsynth91-comp lgsynth91-cordic lgsynth91-count lgsynth91-cu lgsynth91-dalu lgsynth91-decod lgsynth91-des lgsynth91-dsip lgsynth91-example2 lgsynth91-f51m lgsynth91-frg1 lgsynth91-frg2 lgsynth91-i10 lgsynth91-i1 lgsynth91-i2 lgsynth91-i3 lgsynth91-i4 lgsynth91-i5 lgsynth91-i6 lgsynth91-i7 lgsynth91-i8 lgsynth91-i9 lgsynth91-k2 lgsynth91-lal lgsynth91-majority lgsynth91-mm30a lgsynth91-mm4a lgsynth91-mm9a lgsynth91-mm9b lgsynth91-mult16a lgsynth91-mult16b lgsynth91-mult32a lgsynth91-mult32b lgsynth91-mux lgsynth91-my_adder lgsynth91-pair lgsynth91-parity lgsynth91-pcle lgsynth91-pcler8 lgsynth91-pm1 lgsynth91-rot lgsynth91-s1196 lgsynth91-s13207.1 lgsynth91-s1423 lgsynth91-s1488 lgsynth91-s1494 lgsynth91-s15850.1 lgsynth91-s208.1 lgsynth91-s27 lgsynth91-s298 lgsynth91-s344 lgsynth91-s349 lgsynth91-s382 lgsynth91-s38417 lgsynth91-s38584.1 lgsynth91-s386 lgsynth91-s400 lgsynth91-s420.1 lgsynth91-s444 lgsynth91-s510 lgsynth91-s526 lgsynth91-s5378 lgsynth91-s641 lgsynth91-s713 lgsynth91-s820 lgsynth91-s832 lgsynth91-s838.1 lgsynth91-s9234.1 lgsynth91-sbc lgsynth91-sct lgsynth91-t481 lgsynth91-tcon lgsynth91-term1 lgsynth91-too_large lgsynth91-ttt2 lgsynth91-unreg lgsynth91-vda lgsynth91-x1 lgsynth91-x2 lgsynth91-x3 lgsynth91-x4 lgsynth91-z4ml mcnc-5xp1 mcnc-9sym mcnc-9symml mcnc-a mcnc-al2 mcnc-alcom mcnc-alu1 mcnc-alu2 mcnc-alu3 mcnc-alu4 mcnc-amd mcnc-apex1 mcnc-apex2 mcnc-apex3 mcnc-apex4 mcnc-apex5 mcnc-apex6 mcnc-apex7 mcnc-apla mcnc-b10 mcnc-b11 mcnc-b12 mcnc-b1 mcnc-b2 mcnc-b3 mcnc-b4 mcnc-b7 mcnc-b9 mcnc-bbara mcnc-bbsse mcnc-bbtas mcnc-bc0 mcnc-bca mcnc-bcb mcnc-bcc mcnc-bcd mcnc-beecount mcnc-br1 mcnc-br2 mcnc-bw mcnc-C1355 mcnc-C17 mcnc-C1908 mcnc-C2670 mcnc-C3540 mcnc-C432 mcnc-C499 mcnc-C5315 mcnc-C6288 mcnc-C7552 mcnc-C880 mcnc-c8 mcnc-cc mcnc-chkn mcnc-cht mcnc-clip mcnc-clpl mcnc-cm138a mcnc-cm150a mcnc-cm151a mcnc-cm152a mcnc-cm162a mcnc-cm163a mcnc-cm42a mcnc-cm82a mcnc-cm85a mcnc-cmb mcnc-comp mcnc-con1 mcnc-cordic mcnc-count mcnc-cps mcnc-cse mcnc-cu mcnc-dalu mcnc-dc1 mcnc-dc2 mcnc-decod mcnc-dekoder mcnc-des mcnc-dist mcnc-dk14 mcnc-dk15 mcnc-dk16 mcnc-dk17 mcnc-dk27 mcnc-dk48 mcnc-dk512 mcnc-donfile mcnc-duke2 mcnc-e64 mcnc-ex1010 mcnc-ex1 mcnc-ex2 mcnc-ex3 mcnc-ex4 mcnc-ex5 mcnc-ex6 mcnc-ex7 mcnc-example2 mcnc-exep mcnc-exp mcnc-exps mcnc-f51m mcnc-frg1 mcnc-frg2 mcnc-gary mcnc-i10 mcnc-i1 mcnc-i2 mcnc-i3 mcnc-i4 mcnc-i5 mcnc-i6 mcnc-i7 mcnc-i8 mcnc-i9 mcnc-ibm mcnc-in0 mcnc-in1 mcnc-in2 mcnc-in3 mcnc-in4 mcnc-in5 mcnc-in6 mcnc-in7 mcnc-inc mcnc-intb mcnc-jbp mcnc-k2 mcnc-keyb mcnc-kirkman mcnc-lal mcnc-lin mcnc-lion9 mcnc-lion mcnc-luc mcnc-m1 mcnc-m2 mcnc-m3 mcnc-m4 mcnc-mainpla mcnc-majority mcnc-mark1 mcnc-max1024 mcnc-max128 mcnc-max46 mcnc-max512 mcnc-mc mcnc-misex1 mcnc-misex2 mcnc-misex3 mcnc-misex3c mcnc-misg mcnc-mish mcnc-misj mcnc-mlp4 mcnc-modulo12 mcnc-mp2d mcnc-mux mcnc-my_adder mcnc-newapla1 mcnc-newapla2 mcnc-newapla mcnc-newbyte mcnc-newcond mcnc-newcpla1 mcnc-newcpla2 mcnc-newcwp mcnc-newill mcnc-newtag mcnc-newtpla1 mcnc-newtpla2 mcnc-newtpla mcnc-newxcpla1 mcnc-o64 mcnc-opa mcnc-opus mcnc-p82 mcnc-pair mcnc-parity mcnc-pcle mcnc-pcler8 mcnc-pdc mcnc-planet1 mcnc-planet mcnc-pm1 mcnc-pope mcnc-prom1 mcnc-prom2 mcnc-rd53 mcnc-rd73 mcnc-rd84 mcnc-risc mcnc-root mcnc-rot mcnc-ryy6 mcnc-s1a mcnc-s1 mcnc-s8 mcnc-sand mcnc-sao2 mcnc-scf mcnc-sct mcnc-seq mcnc-sex mcnc-shift mcnc-shiftreg mcnc-signet mcnc-soar mcnc-spla mcnc-sqn mcnc-sqr6 mcnc-sqrt8 mcnc-squar5 mcnc-sse mcnc-styr mcnc-t1 mcnc-t2 mcnc-t3 mcnc-t481 mcnc-t4 mcnc-table3 mcnc-table5 mcnc-tav mcnc-tbk mcnc-t mcnc-tcon mcnc-term1 mcnc-ti mcnc-tms mcnc-too_large mcnc-train11 mcnc-train4 mcnc-ts10 mcnc-ttt2 mcnc-unreg mcnc-vda mcnc-vg2 mcnc-vtx1 mcnc-wim mcnc-x1 mcnc-x1dn mcnc-x2 mcnc-x2dn mcnc-x3 mcnc-x4 mcnc-x6dn mcnc-x7dn mcnc-x9dn mcnc-xor5 mcnc-xparc mcnc-z4ml mcnc-Z5xp1 mcnc-Z9sym'

set xlabel font "Helvetica,24"
set ylabel font "Helvetica,24"
set xrange [0:100]
set yrange [0:100]
set key off

set xlabel 'Approximation (%)'
set ylabel 'Corruptibility (%)'

set output 'figures/estimate.png'
plot 'estimate.csv' using 3:4 with points

file_exists(file) = system("[ -f '".file."' ] && echo '1' || echo '0'") + 0

do for [name in list] {
	datafile='benchmarks/estimate/'.name.'.csv'
	if (file_exists(datafile)) {
		set output 'figures/estimate_'.name.'.png'
		plot datafile using 3:4 with points
	}
}


set xlabel 'Area penalty (%)'
set ylabel 'Corruptibility (%)'

do for [name in list] {
	datafile='benchmarks/area/'.name.'.csv'
	if (file_exists(datafile)) {
		set output 'figures/area_'.name.'.png'
		plot datafile using 2:3 with linespoints
	}
}


set xlabel 'Delay penalty (%)'
set ylabel 'Corruptibility (%)'

do for [name in list] {
	datafile='benchmarks/delay/'.name.'.csv'
	if (file_exists(datafile)) {
		set output 'figures/delay_'.name.'.png'
		plot datafile using 2:3 with linespoints
	}
}


set xlabel 'Area penalty (%)'
set ylabel 'Pairwise security (bits)'

set yrange [0:*]

do for [name in list] {
	datafile='benchmarks/area_pairwise/'.name.'.csv'
	if (file_exists(datafile)) {
		set output 'figures/area_pairwise_'.name.'.png'
		plot datafile using 2:3 with linespoints
	}
}


set xlabel 'Delay penalty (%)'
set ylabel 'Pairwise security (bits)'

set yrange [0:*]

do for [name in list] {
	datafile='benchmarks/delay_pairwise/'.name.'.csv'
	if (file_exists(datafile)) {
		set output 'figures/delay_pairwise_'.name.'.png'
		plot datafile using 2:3 with linespoints
	}
}


