# C++ Process Monitor

Developed using libproc.h and mach for MacOS (This Process Monitor will not work on Linux or Windows)

The display functionality uses ncurses

Functionalities to be improved upon:
1) UsedRAM value is currently inaccurate, need to improve upon this calculation (Currently getting up to 1GB disparity with the Mac Activity Monitor)
2) Can improve the "Group-By-Name" functionality

To compile and run the process monitor, run:
```
make
./process_monitor
```

