#!/bin/bash

nproc=4

for N in $(seq 50 50 200); do
  for p in $(seq 0.1 0.1 0.9); do
    mpirun -np $nproc ./forest_fire $N $p 50
  done
done


