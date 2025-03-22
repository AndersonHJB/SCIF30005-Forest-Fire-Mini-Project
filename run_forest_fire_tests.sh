#!/bin/bash

nproc=4  # 设置进程数

for N in $(seq 50 50 200); do
  for p in $(seq 0.1 0.1 0.9); do
    filename="output/output_N${N}_p${p}.txt"
    echo "Running N=$N, p=$p, M=50" > "$filename"
    mpirun -np $nproc ./forest_fire $N $p 50 >> "$filename" 2>&1
  done
done