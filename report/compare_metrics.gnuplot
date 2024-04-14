
set terminal 'pngcairo' size 800,600

set datafile separator ' '

set xlabel font "Helvetica,24"
set ylabel font "Helvetica,24"
set key font "Helvetica,20"
set yrange [0:100]
set key left

set xlabel 'Testcases'
set ylabel 'Corruptibility (%)'

set output 'figures/compare_metrics.png'

plot 'benchmarks/corruptibility_corruption.csv' using ($0):3 with lines title "Moosic", \
     'benchmarks/corruptibility_fll.csv' using ($0):3 with lines title "FLL", \
     'benchmarks/corruptibility_kip.csv' using ($0):3 with lines title "KIP"
