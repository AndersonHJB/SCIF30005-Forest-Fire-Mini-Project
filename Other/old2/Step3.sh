#!/usr/bin/env bash
executable="./forest_fire"  # 你的并行可执行文件
p=0.6                       # 初始树密度
M=50                        # 重复次数
N_list=(50 100 500)         # 要测试的网格规模
procs_list=(1 2 4 8 16)     # 要测试的进程数
output_csv="performance_results.csv"

echo "N,nprocs,avg_steps,fraction_reached_bottom,avg_time_seconds,speedup,efficiency" > "$output_csv"


for N in "${N_list[@]}"; do

    echo "Running baseline (nprocs=1) for N=$N..."
    # 用 mpirun -np 1 执行一次
    baseline_output=$(mpirun -np 1 "${executable}" "$N" "$p" "$M")

    avg_steps_base=$(echo "$baseline_output" | grep "Average steps" | awk '{print $6}')
    frac_reach_base=$(echo "$baseline_output" | grep "Fraction of runs" | awk '{print $7}')
    avg_time_base=$(echo "$baseline_output" | grep "Average wall time" | awk '{print $7}')  # 单位: s

    # speedup(1) = 1.0, efficiency(1) = 1.0
    speedup_base=1.0
    efficiency_base=1.0
    echo "$N,1,$avg_steps_base,$frac_reach_base,$avg_time_base,$speedup_base,$efficiency_base" >> "$output_csv"

    T1="$avg_time_base"

    for np in "${procs_list[@]}"; do
        # 跳过已经跑过的 nprocs=1
        [ "$np" -eq 1 ] && continue

        echo "Running test with nprocs=$np for N=$N..."
        current_output=$(mpirun -np "$np" "${executable}" "$N" "$p" "$M")

        # 解析输出
        avg_steps=$(echo "$current_output" | grep "Average steps" | awk '{print $6}')
        fraction_reached=$(echo "$current_output" | grep "Fraction of runs" | awk '{print $7}')
        avg_time=$(echo "$current_output" | grep "Average wall time" | awk '{print $7}')  # s

        # 计算 speedup = T(1) / T(np)， efficiency = speedup / np
        speedup=$(awk -v t1="$T1" -v tp="$avg_time" 'BEGIN {print t1/tp}')
        efficiency=$(awk -v sp="$speedup" -v p="$np" 'BEGIN {print sp/p}')

        echo "$N,$np,$avg_steps,$fraction_reached,$avg_time,$speedup,$efficiency" >> "$output_csv"
    done

done

echo "All tests completed. Results saved in $output_csv."
