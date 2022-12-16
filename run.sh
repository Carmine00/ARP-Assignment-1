#!/bin/bash
#Script to compile all the processes and run the master
gcc src/master.c -o bin/master
gcc src/command_console.c -lncurses -o bin/command_console
gcc src/inspection.c -lncurses -lm -o bin/inspection 
gcc src/real.c -o bin/real
gcc src/M2.c -o bin/M2 
gcc src/M1.c -o bin/M1 
gcc src/watchdog.c -o bin/watchdog 
./bin/master
exit 0


