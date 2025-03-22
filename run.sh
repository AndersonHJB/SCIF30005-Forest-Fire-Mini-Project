#!/bin/bash

# ---------------------------------------
# Forest Fire MPI Simulation - Test Suite
# 对应课程：SCIF30005 Mini Project
# 测试内容：
#  - 功能正确性
#  - 模型收敛性 (M vs p)
#  - 并行性能测试 (不同 N, 不同进程数)
# ---------------------------------------

echo "编译程序..."
mpicxx -std=c++11 -O2 forest_fire.cpp -o forest_fire
echo "编译完成。"

# ---------------------------------------
# ✅ 功能测试 1：读取初始网格文件进行模拟
# 对应题目：
# - the ability to read in an initial grid from a text file
# ---------------------------------------
echo "测试：从 input_grid.txt 文件读取网格..."
mpirun -np 4 ./forest_fire 6 0.0 1 input_grid.txt

# ---------------------------------------
# ✅ 功能测试 2：生成随机网格并模拟一次
# 对应题目：
# - the ability to generate a random starting grid of size N with probability p
# ---------------------------------------
echo "测试：随机生成 10x10 网格，p=0.5，运行一次..."
mpirun -np 2 ./forest_fire 10 0.5 1

# ---------------------------------------
# ✅ 模型收敛性分析：多次运行，固定 N=100
# 对应题目：
# - is M = 50 a sufficient number of repeats to reach convergence?
# - does the answer depend on the initial probability p?
# ---------------------------------------
echo "测试：固定 N=100，M=50，多个 p 值，测试是否收敛..."

for p in 0.1 0.3 0.5 0.6 0.7 0.9
do
  echo "测试 p=$p"
  mpirun -np 4 ./forest_fire 100 $p 50
done

# （可扩展脚本自动测试 M=10/20/30/50/...）

# ---------------------------------------
# ✅ 并行性能测试：不同 N，固定 p=0.6，M=50
# 对应题目：
# - Performance Analysis: N = 50, 100, 500
# - how performance varies with number of MPI tasks
# ---------------------------------------
echo "性能测试：不同网格大小 N=50,100,500，p=0.6，M=50，测试不同进程数..."

# 测试配置：每个 N 配多种进程数
declare -a nsizes=(50 100 500)
declare -a procs=(1 2 4 8 16)

for N in "${nsizes[@]}"
do
  for np in "${procs[@]}"
  do
    echo "运行：N=$N, p=0.6, M=50, 进程数=$np"
    mpirun -np $np ./forest_fire $N 0.6 50
  done
done

# ✅ END
echo "✅ 所有测试完成！"
