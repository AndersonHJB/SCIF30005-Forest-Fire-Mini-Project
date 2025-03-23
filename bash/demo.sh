#!/usr/bin/env bash

N=100
NP=4
ARRAY_P=(0.1 0.3 0.5 0.7 0.9)
ARRAY_M=(10 20 30 40 50 60 70 80 90 100)
EXEC=./forest_fire

echo "======================================="
echo " Part 1: 多个 p 值 (M=50) 测试"
echo "---------------------------------------"
for p in "${ARRAY_P[@]}"; do
    echo "Running: N=${N}, p=${p}, M=50 with -np ${NP}"
    mpirun -np ${NP} ${EXEC} ${N} ${p} 50
    echo
done

echo "======================================="
echo " Part 2: 对 p=0.6 不同 M 测试(收敛分析)"
echo "---------------------------------------"
for M in "${ARRAY_M[@]}"; do
    echo "Running: N=${N}, p=0.6, M=${M} with -np ${NP}"
    mpirun -np ${NP} ${EXEC} ${N} 0.6 ${M}
    echo
done

echo "Done. All tests completed."
