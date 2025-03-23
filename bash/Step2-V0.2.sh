#!/usr/bin/env bash
#
# 该脚本演示如何进行 "多次重复(M次) 测试收敛性" 并将结果保存为CSV文件。
# 1) 固定 N=100, 针对多个 p (0.1,0.3,0.5,0.7,0.9) 进行 M=50 的测试
# 2) 对 p=0.6，测试 M=10,20,30,40,50,60,80,100 等值
# 最终将所有测试结果汇总到 results_convergence.csv。
#
# 使用方法（示例）:
#   chmod +x test_convergence_to_csv.sh
#   ./test_convergence_to_csv.sh
#
# 注意:
#   - 需要确保脚本里指定的可执行文件 ./forest_fire 存在且可运行
#   - 如果是在 HPC 上，请根据集群环境改写 mpirun/作业脚本提交方式。
#

#############################
# 1. 配置测试参数 (可自定义)
#############################
N=100                # 网格大小
NP=4                 # 使用多少个 MPI 进程
EXEC=./forest_fire   # 可执行文件路径
CSV_FILE="results_convergence.csv"

# 不同 p 的列表 (固定 M=50)
ARRAY_P=(0.1 0.3 0.5 0.7 0.9)

# 针对 p=0.6，测试一系列 M (用于详细收敛分析)
ARRAY_M=(10 20 30 40 50 60 70 80 90 100)

#################################
# 2. 输出CSV文件头（若需要的话）
#################################
# CSV列的含义: N, p, M, AvgSteps, FractionReachedBottom, AvgWallTime
echo "N,p,M,avg_steps,fraction_reached_bottom,avg_wall_time_s" > "$CSV_FILE"

#################################
# 3. 第1部分: 多个 p (M=50)
#################################
for p in "${ARRAY_P[@]}"; do
    echo "Running [N=$N, p=$p, M=50] with -np $NP..."
    # 运行命令并捕获输出
    OUTPUT=$(mpirun -np ${NP} ${EXEC} ${N} ${p} 50)

    # 解析关键信息
    # 提取 N
    N_VAL=$(echo "$OUTPUT" | grep "N =" | sed -n 's/.*N = \([0-9]*\).*/\1/p')
    # 提取 p
    P_VAL=$(echo "$OUTPUT" | grep "N =" | sed -n 's/.*p = \([0-9.]*\).*/\1/p')
    # 提取 M
    M_VAL=$(echo "$OUTPUT" | grep "N =" | sed -n 's/.*M = \([0-9]*\).*/\1/p')
    # 提取 Average steps
    STEPS=$(echo "$OUTPUT" | grep "Average steps" | sed -n 's/.*: \([0-9.]*\)/\1/p')
    # 提取 Fraction of runs
    FRACTION=$(echo "$OUTPUT" | grep "Fraction of runs" | sed -n 's/.*: \([0-9.]*\)/\1/p')
    # 提取 Average wall time
    TIME=$(echo "$OUTPUT" | grep "Average wall time" | sed -n 's/.*: \([0-9.]*\) s.*/\1/p')

    # 写入 CSV
    echo "${N_VAL},${P_VAL},${M_VAL},${STEPS},${FRACTION},${TIME}" >> "$CSV_FILE"

    echo "$OUTPUT"   # 也可以把 OUTPUT 打印到终端以便查看
    echo            # 分割行
done

#################################
# 4. 第2部分: p=0.6, 不同 M
#################################
p_fixed=0.6
for M in "${ARRAY_M[@]}"; do
    echo "Running [N=$N, p=$p_fixed, M=$M] with -np $NP..."
    OUTPUT=$(mpirun -np ${NP} ${EXEC} ${N} ${p_fixed} ${M})

    # 同样解析
    N_VAL=$(echo "$OUTPUT" | grep "N =" | sed -n 's/.*N = \([0-9]*\).*/\1/p')
    P_VAL=$(echo "$OUTPUT" | grep "N =" | sed -n 's/.*p = \([0-9.]*\).*/\1/p')
    M_VAL=$(echo "$OUTPUT" | grep "N =" | sed -n 's/.*M = \([0-9]*\).*/\1/p')
    STEPS=$(echo "$OUTPUT" | grep "Average steps" | sed -n 's/.*: \([0-9.]*\)/\1/p')
    FRACTION=$(echo "$OUTPUT" | grep "Fraction of runs" | sed -n 's/.*: \([0-9.]*\)/\1/p')
    TIME=$(echo "$OUTPUT" | grep "Average wall time" | sed -n 's/.*: \([0-9.]*\) s.*/\1/p')

    echo "${N_VAL},${P_VAL},${M_VAL},${STEPS},${FRACTION},${TIME}" >> "$CSV_FILE"

    echo "$OUTPUT"
    echo
done

echo "==========================================="
echo "All tests completed. Results saved to $CSV_FILE"
echo "==========================================="
