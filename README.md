# 长期招收一对一学员、作业代写

微信：Jiabcdefh

# 运行命令

## 1. 编译命令

```bash
mpicxx -std=c++11 -O2 forest_fire.cpp -o forest_fire
# or
mpicxx -std=c++14 -O2 forest_fire.cpp -o forest_fire
```

- `mpicxx`：MPI 的 C++ 编译器。
- `-std=c++11`：启用 C++11 标准（使用 lambda、auto 等语法必需）。
- `-O2`：编译优化等级。
- `forest_fire.cpp`：源文件名。 
- `-o forest_fire`：输出可执行文件名。

## 2. 测试命令

1. 从输入文件读取网格并单次测试

```bash
mpirun -np 4 ./forest_fire 6 0.0 1 input_grid.txt
```



















