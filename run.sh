#!/bin/bash

# 编译代码
make

# 运行收敛性研究
echo "正在运行收敛性研究..."
mpirun -np 4 ./forest_fire 3 > convergence_results.csv

# 使用不同进程数运行性能分析
echo "正在运行性能分析..."
for procs in 1 2 4 8 16
do
    mpirun -np $procs ./forest_fire 4 >> performance_results.csv
done

echo "所有测试已完成。"