#!/usr/bin/env bash

# ------------------------------------------------------------
# 配置部分
# ------------------------------------------------------------
executable="./forest_fire"  # 你的并行可执行文件
p=0.6                       # 初始树密度
M=50                        # 重复次数
N_list=(50 100 500)         # 要测试的网格规模
procs_list=(1 2 4 8 16)     # 要测试的进程数
output_csv="performance_results.csv"

# ------------------------------------------------------------
# 初始化CSV文件头
# ------------------------------------------------------------
# CSV列: N,nprocs,avg_steps,fraction_reached_bottom,avg_time_seconds,speedup,efficiency
echo "N,nprocs,avg_steps,fraction_reached_bottom,avg_time_seconds,speedup,efficiency" > "$output_csv"


# ------------------------------------------------------------
# 主循环：对不同 N 和 nprocs 进行测试
# ------------------------------------------------------------
for N in "${N_list[@]}"; do

    # --------------------------------------------------------
    # 首先在 nprocs=1 时获取参考时间 T(1) (用于Speedup计算)
    # --------------------------------------------------------
    echo "Running baseline (nprocs=1) for N=$N..."
    # 用 mpirun -np 1 执行一次
    baseline_output=$(mpirun -np 1 "${executable}" "$N" "$p" "$M")

    # 解析 baseline 输出
    # 假设输出中有类似:
    #   N = 100, p = 0.6, M = 50
    #   Average steps before fire stops: 123.45
    #   Fraction of runs that reached bottom: 0.76
    #   Average wall time (max among procs): 0.123 s
    avg_steps_base=$(echo "$baseline_output" | grep "Average steps" | awk '{print $6}')
    frac_reach_base=$(echo "$baseline_output" | grep "Fraction of runs" | awk '{print $7}')
    avg_time_base=$(echo "$baseline_output" | grep "Average wall time" | awk '{print $7}')  # 单位: s

    # speedup(1) = 1.0, efficiency(1) = 1.0
    speedup_base=1.0
    efficiency_base=1.0

    # 写入 CSV (nprocs=1这条)
    echo "$N,1,$avg_steps_base,$frac_reach_base,$avg_time_base,$speedup_base,$efficiency_base" >> "$output_csv"

    # 我们将 avg_time_base 作为计算 speedup 的 T(1)
    T1="$avg_time_base"

    # --------------------------------------------------------
    # 其余进程数 nprocs=2,4,8...
    # --------------------------------------------------------
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

        # 写入CSV
        echo "$N,$np,$avg_steps,$fraction_reached,$avg_time,$speedup,$efficiency" >> "$output_csv"
    done

done

echo "All tests completed. Results saved in $output_csv."
