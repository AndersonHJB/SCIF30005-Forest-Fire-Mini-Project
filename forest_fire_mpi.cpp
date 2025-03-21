#include <mpi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <random>
#include <sstream>

// 状态常量：EMPTY=0, TREE=1, BURNING=2, BURNT=-1(或3亦可)
static const int EMPTY   = 0;
static const int TREE    = 1;
static const int BURNING = 2;
static const int BURNT   = -1;

// 使用的结构体，用于收集某次模拟的结果
struct SimulationResult {
    int steps;
    bool reachedBottom;
    double timeUsed;
};

// 在局部网格里（含边界幽灵行）计算某个格点在下一步的状态
int nextState(int currentState, int up, int down, int left, int right) {
    // 如果当前是树（TREE），并且四邻中任意一个为 BURNING，则该树变为 BURNING
    if (currentState == TREE) {
        if (up == BURNING || down == BURNING || left == BURNING || right == BURNING) {
            return BURNING;
        } else {
            return TREE;
        }
    }
        // 如果当前是 BURNING，则下一步就变成 BURNT
    else if (currentState == BURNING) {
        return BURNT;
    }
        // 其余情况不变（EMPTY 或 BURNT 都保持原样）
    else {
        return currentState;
    }
}

// 从文本文件读入一个 N×N 网格（只在 rank=0 上读）
bool readGridFromFile(const std::string &filename, std::vector<int> &grid, int N) {
    std::ifstream fin(filename.c_str());
    if (!fin.is_open()) {
        return false;
    }
    // 按行列顺序读取
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            int val;
            fin >> val;
            grid[i*N + j] = val;
        }
    }
    fin.close();
    return true;
}

// 根据概率 p 随机生成 N×N 网格
void generateRandomGrid(std::vector<int> &grid, int N, double p, unsigned int seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    for (int i = 0; i < N*N; i++) {
        double r = dist(gen);
        if (r < p) {
            grid[i] = TREE;
        } else {
            grid[i] = EMPTY;
        }
    }
}

// 在网格顶部点燃所有树
void igniteTopRow(std::vector<int> &grid, int N) {
    for (int j = 0; j < N; j++) {
        if (grid[j] == TREE) {
            grid[j] = BURNING;
        }
    }
}

// 单次模拟（单次 run）：给定初始 grid，在并行环境下进行森林火灾模拟
SimulationResult runSimulationParallel(const std::vector<int> &initialGrid, int N,
                                       MPI_Comm comm)
{
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    int baseRows = N / size;       // 每个进程至少要分多少行
    int remainder = N % size;      // 剩余行数
    int localRows = (rank < remainder) ? (baseRows + 1) : baseRows;


    int startRow = 0;
    for (int r = 0; r < rank; r++) {
        startRow += (r < remainder) ? (baseRows + 1) : baseRows;
    }
    int endRow = startRow + localRows - 1;  // inclusive

    std::vector<int> currentLocal((localRows+2)*N, EMPTY);
    std::vector<int> nextLocal((localRows+2)*N, EMPTY);

    if (rank == 0) {
        for (int i = 0; i < localRows; i++) {
            for (int j = 0; j < N; j++) {
                currentLocal[(i+1)*N + j] = initialGrid[(startRow + i)*N + j];
            }
        }
        for (int r = 1; r < size; r++) {
            int sRow = 0;
            for (int rr = 0; rr < r; rr++) {
                sRow += (rr < remainder) ? (baseRows + 1) : baseRows;
            }
            int lRows = (r < remainder) ? (baseRows + 1) : baseRows;
            if (lRows > 0) {
                MPI_Send(&initialGrid[sRow*N], lRows*N, MPI_INT, r, 0, comm);
            }
        }
    } else {
        MPI_Recv(&currentLocal[N], localRows*N, MPI_INT, 0, 0, comm, MPI_STATUS_IGNORE);
    }

}

// 主函数
int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // 命令行参数解析（简单示例）：
    // 用法： mpirun -n <np> ./forest_fire N p M seed [input_file(optional)]
    //   N: 网格尺寸
    //   p: 随机生成时，种树的概率
    //   M: 重复运行次数
    //   seed: 随机数种子（用于生成不同网格）
    //   input_file(可选): 如果提供该文件名，则从文件读入初始网格（只执行1次，不会随机）
    if (argc < 5) {
        if (rank == 0) {
            std::cerr << "Usage: mpirun -n <np> ./forest_fire N p M seed [input_file]" << std::endl;
        }
        MPI_Finalize();
        return 1;
    }

    int N = std::atoi(argv[1]);
    double p = std::atof(argv[2]);
    int M = std::atoi(argv[3]);
    unsigned int seed = (unsigned int)std::atoi(argv[4]);
    std::string filename = (argc > 5) ? argv[5] : "";

    // 如果提供了文件名，则仅执行一次模拟，并从文件读入初始网格
    bool useFile = !filename.empty();

    // 准备收集所有重复运行的结果（仅 rank=0 用）
    std::vector<SimulationResult> allResults(M);

    // 如果使用文件输入，就只跑一次，不再做随机多次
    int actualRuns = useFile ? 1 : M;

    // 对每一次独立运行：
    for (int runIdx = 0; runIdx < actualRuns; runIdx++) {

        // rank=0 生成/读入网格
        std::vector<int> grid(N*N, EMPTY);
        if (rank == 0) {
            if (useFile) {
                // 从文件读入初始网格
                bool ok = readGridFromFile(filename, grid, N);
                if (!ok) {
                    std::cerr << "Failed to open or read file: " << filename << std::endl;
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
            } else {
                // 随机生成
                // 这里为保证多次run不重复，可以加上 (seed + runIdx*12345) 做种子偏移
                generateRandomGrid(grid, N, p, seed + runIdx*12345);
            }
            // 点燃顶部的树
            igniteTopRow(grid, N);
        }

        // 并行运行模拟
        SimulationResult res = runSimulationParallel(grid, N, MPI_COMM_WORLD);

        // 将结果收集到 rank=0
        if (!useFile) {
            // 如果是多次随机运行，需要收集到0进程
            if (rank == 0) {
                allResults[runIdx] = res;
            }
        } else {
            // 如果使用文件输入，只跑一次
            if (rank == 0) {
                allResults[0] = res;
            }
        }
    }

    // 输出结果
    if (rank == 0) {
        // 如果使用文件输入：直接输出那一次的结果
        if (useFile) {
            const SimulationResult &r = allResults[0];
            std::cout << "=== Single run (from file) ===" << std::endl;
            std::cout << "N = " << N << "\n";
            std::cout << "Steps until fire stops = " << r.steps << "\n";
            std::cout << "Fire reached bottom? " << (r.reachedBottom ? "Yes" : "No") << "\n";
            std::cout << "Time used (s) = " << r.timeUsed << "\n";
        } else {
            // 多次随机运行：计算平均步数、烧到底部几次、平均耗时
            double avgSteps = 0.0;
            int bottomCount = 0;
            double avgTime = 0.0;
            for (int i = 0; i < M; i++) {
                avgSteps += allResults[i].steps;
                if (allResults[i].reachedBottom) {
                    bottomCount++;
                }
                avgTime += allResults[i].timeUsed;
            }
            avgSteps /= (double)M;
            avgTime  /= (double)M;

            std::cout << "=== Forest Fire Simulation (MPI) ===" << std::endl;
            std::cout << "N = " << N << ", p = " << p << ", M = " << M << std::endl;
            std::cout << "Average steps until fire stops = " << avgSteps << std::endl;
            std::cout << "Fraction of runs that reached bottom = "
                      << bottomCount << "/" << M << std::endl;
            std::cout << "Average time used (s) = " << avgTime << std::endl;
        }
    }

    MPI_Finalize();
    return 0;
}