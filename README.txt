7800heat readme

7800heat is a profiling utility, to help you track down what parts of your
code are executed the most, and ideal candidates for tuning.

7800heat takes as input a a7800/MAME emulator trace file, and generates
a heat-map HTML file, that shows execution count of your instructions,
in addition to coloring each line in a hue somewhere from blue (least-
executed) to red. (most-executed)

Running 7800heat
----------------
Usage: ./7800heat -t TRACEFILE.trc -o OUTFILE.html [-f]

  -t TRACEFILE.trc specifies the trace file to parse.
  -o OUTFILE.html specifies the output html heatmap.
  -f specifies full trace counts (include tight loops).

To generate the trace file:

  1.  Launch the game in the a7800 debugger:
        a7800 a7800 -cart mygame.a78 -debug
  2.  In the debug window type:
        trace gamename.trc,0,noloop
  2a. If your game is SuperGame format, type:
        wpset 8000,4000,w,1==1,{tracelog "BANK: %02X\n",wpdata;g}
  3.  Hit F5 in the debug window to run the game, and play for a few minutes.
  4.  Hit F11 in the debug window to pause execution, and type:
        trace off.


Legal Stuff
-----------
 7800heat is created by Mike Saarna, copyright 2020. It's provided here under
 the GPL v2 license. See the included LICENSE.txt file for details.
