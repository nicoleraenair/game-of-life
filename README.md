# game-of-life

A command line application that implements Conway's Game of Life (with multithreading for efficiency).

The program takes an input file as a command line argument, parses the file to initialize a game board struct, spawns threads to execute gameplay in parallel, and outputs an animation of the changing board.

It makes use of the POSIX Threads API - particularly spawning/joining threads, locking with mutexes, and synchronization with barriers. As macOS does not have an implementation for pthread_barrier, a header file containing an [implementation of barriers by Albert Armea](https://blog.albertarmea.com/post/47089939939/using-pthreadbarrier-on-mac-os-x) was included in the project.

### Usage
To compile: gcc gol.c -o gol -lpthread\
To run: ./gol inputfile.txt

### Input File Format:
Input files should be formatted as follows (where each (i, j) pair represents the coordinates of an initial live cell):

```
  number of rows
  number of cols
  number of rounds to simulate
  number of cells that are initially alive
  i j
  i j
  ...
```

See test files windmill.txt, small.txt, big.txt, corners.txt, and edges.txt for examples


