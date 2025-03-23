#!/bin/bash

# 固定 N=100，M=50，测试 p=0.1, 0.3, 0.5, 0.7, 0.9
# 使用 4 个进程并行
# 收集每次输出中的平均步数、到达底部概率、平均耗时等
echo "p,steps,reach,time" > result_p_study.csv
for p in 0.1 0.3 0.5 0.7 0.9
do
    output=$(mpirun -np 4 ./forest_fire 100 $p 50)
    # 从输出文本中提取关键信息：
    steps=$(echo "$output" | grep "Average steps before fire stops" | awk '{print $NF}')
    reach=$(echo "$output" | grep "Fraction of runs that reached bottom" | awk '{print $NF}')
    time=$(echo "$output" | grep "Average wall time (max among procs)" | awk '{print $NF}')

    echo "$p,$steps,$reach,$time" >> result_p_study.csv
done
