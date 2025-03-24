#!/bin/bash

nproc=4
csv_file="Implementation.csv"

# 初始化 CSV 表头
echo "N,p,M,avg_steps,fraction_reached_bottom,avg_wall_time_s" > "$csv_file"

for N in $(seq 50 50 200); do
  for p in $(seq 0.1 0.1 0.9); do
    echo "Running N=$N, p=$p, M=50"

    # 捕获程序输出
    output=$(mpirun -np $nproc ./forest_fire $N $p 50)

    # 提取信息
    N_val=$(echo "$output" | grep "N =" | sed -n 's/.*N = \([0-9]*\).*/\1/p')
    p_val=$(echo "$output" | grep "p =" | sed -n 's/.*p = \([0-9.]*\).*/\1/p')
    M_val=$(echo "$output" | grep "M =" | sed -n 's/.*M = \([0-9]*\).*/\1/p')
    steps=$(echo "$output" | grep "Average steps" | sed -n 's/.*: \([0-9.]*\)/\1/p')
    fraction=$(echo "$output" | grep "Fraction of runs" | sed -n 's/.*: \([0-9.]*\)/\1/p')
    walltime=$(echo "$output" | grep "Average wall time" | sed -n 's/.*: \([0-9.]*\) s.*/\1/p')

    # 写入 CSV 行
    echo "${N_val},${p_val},${M_val},${steps},${fraction},${walltime}" >> "$csv_file"
  done
done

echo "✅ All tests completed. Results saved to $csv_file"
