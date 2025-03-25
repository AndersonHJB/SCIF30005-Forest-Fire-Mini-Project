#!/usr/bin/env bash

# 输出 CSV 文件名
OUTFILE="performance_results.csv"

# 如果已经存在旧文件，可选择先删除或重命名，避免数据覆盖
if [ -f "$OUTFILE" ]; then
    echo "Removing old $OUTFILE"
    rm -f "$OUTFILE"
fi

# 写入表头(列名)
echo "N,nprocs,avg_steps,fraction_reached_bottom,avg_time_seconds" >> "$OUTFILE"

# 参数设置
# 三个 N 值
NVALUES="50 100 500"
# 并行进程数
NPVALUES="1 2 4 8 16 32"

# 固定 p=0.6, M=50 (可根据需要自行修改)
P=0.6
M=50

# 循环测试
for N in $NVALUES; do
  for np in $NPVALUES; do
    echo "Running forest_fire with N=$N, p=$P, M=$M using $np MPI processes..."

    # 运行并捕获输出
    OUT=$(mpirun -np $np ./forest_fire $N $P $M)

    # 若需要更多调试信息，可先 echo "$OUT"

    # 从输出中提取所需指标
    # 假设输出如下几行（示例）：
    #   Average steps before fire stops: 123.45
    #   Fraction of runs that reached bottom: 0.67
    #   Average wall time (max among procs): 0.89 s

    STEPS=$(echo "$OUT" | grep "Average steps before fire stops:" | awk '{print $6}')
    FRAC=$(echo "$OUT" | grep "Fraction of runs that reached bottom:" | awk '{print $7}')
    TIME=$(echo "$OUT" | grep "Average wall time (max among procs):" | awk '{print $8}')

    # 去除TIME中的 "s" 字符，仅保留数值
    # 比如 "0.89 s" -> "0.89"
    TIME=$(echo "$TIME" | sed 's/s//g')

    # 将数据写入 CSV
    echo "$N,$np,$STEPS,$FRAC,$TIME" >> "$OUTFILE"

  done
done

echo "All performance test results have been saved to $OUTFILE."
