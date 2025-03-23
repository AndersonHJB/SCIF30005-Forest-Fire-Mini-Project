#!/bin/bash

nproc=4  # 设置进程数
output_file="all_results.txt"
> "$output_file"

for N in $(seq 50 50 200); do
  for p in $(seq 0.1 0.1 0.9); do
    echo "Running N=$N, p=$p, M=50" >> "$output_file"
    mpirun -np $nproc ./forest_fire $N $p 50 >> "$output_file" 2>&1
    echo "-----------------------------" >> "$output_file"
  done
done