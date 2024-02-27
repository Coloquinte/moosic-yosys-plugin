if (!exists("datafile")) datafile='default.dat'

set datafile separator ',';
set key off;
firstrow = system('head -1 '.datafile.' | sed "s/,/ /g"');
nc = words(firstrow);

if (nc > 4) {
  set tics font "Helvetica,10";
  set label font "Helvetica,10";
  set xlabel font "Helvetica,10";
  set ylabel font "Helvetica,10";

  set multiplot layout nc-2,nc-2;
  do for [i=2:nc-1] {
    do for [j=2:nc-1] {
      set xlabel word(firstrow, i);
      set ylabel word(firstrow, j);
      set xrange [0:]
      set yrange [0:]
      plot datafile using i:j with points;
    }
  }
}
else {
  set xlabel word(firstrow, 2);
  set ylabel word(firstrow, 3);
  set tics font "Helvetica,20"
  set label font "Helvetica,20"
  set xlabel font "Helvetica,20"
  set ylabel font "Helvetica,20"
  set xrange [0:]
  set yrange [0:]
  plot datafile using 2:3 with points;
}

