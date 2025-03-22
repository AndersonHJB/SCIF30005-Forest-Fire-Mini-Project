#include <mpi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <cstdlib>
#include <ctime>
#include <algorithm>

/*
 * 状态编码：
 *   0 -> 空 (empty)
 *   1 -> 活树 (living tree)
 *   2 -> 燃烧 (burning)
 *   3 -> 死树 (dead)
 *
 * 该程序实现了一个火灾模型的并行版本。
 * 使用方法 (示例):
 *   mpirun -np 4 ./forest_fire N p M [optional_input_file]
 *
 * 参数说明:
 *   - N: 网格大小(正方形), N x N
 *   - p: 随机生成时, 每个单元格有 p 的概率为活树
 *   - M: 重复运行次数(用于统计平均)
 *   - optional_input_file: 如果指定了文件名, 则从文件读取网格;
 *       否则基于 N 和 p 随机生成网格
 *
 * 输出:
 *   - 总重复次数 M 的平均模拟步数
 *   - 平均是否到达底部(统计 M 次中有多少次火到达底部)
 *   - 平均运行时长等信息
 */

static const int EMPTY   = 0;
static const int TREE    = 1;
static const int BURNING = 2;
static const int DEAD    = 3;

/**
 * 从文本文件读取网格（如果提供了输入文件）。
 * 文件中每行包含 N 个数字(0/1/...)，用空格分隔。
 * 返回 size=N*N 的一维数组形式存储的网格。
 */
std::vector<int> readGridFromFile(const std::string &filename, int N) {
    std::ifstream fin(filename);
    if(!fin.is_open()) {
        std::cerr << "Error opening grid file: " << filename << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    std::vector<int> grid(N*N, 0);
    for(int r = 0; r < N; r++) {
        for(int c = 0; c < N; c++) {
            if(!(fin >> grid[r*N + c])) {
                std::cerr << "Error reading data from file: not enough entries." << std::endl;
                MPI_Abort(MPI_COMM_WORLD, -1);
            }
        }
    }
    fin.close();
    return grid;
}

/**
 * 基于给定的 N 和 p 随机生成一个网格，返回 size=N*N 的一维数组。
 */
std::vector<int> generateRandomGrid(int N, double p, unsigned int seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    std::vector<int> grid(N*N, 0);
    for(int i = 0; i < N*N; i++) {
        double rnd = dist(gen);
        if(rnd < p) {
            grid[i] = TREE;
        } else {
            grid[i] = EMPTY;
        }
    }
    return grid;
}

/**
 * 进行一次模拟，基于“当前网格状态”，在本地子域上执行火灾扩散。
 * 返回：
 *   - steps: 火焰停止前总步数
 *   - reachedBottom: 是否到达底部(只需判断是否有任意单元在底部行着火过)
 *
 * 注意：这里要把网格在行方向上分块给各进程。通过交换上下边界来更新。
 */
std::pair<int,bool> runSimulationOnce(std::vector<int> &globalGrid,
                                      int N,
                                      int rank,
                                      int size)
{
    // 每个进程负责 startRow ~ endRow-1 (共 localRows 行)
    int rowsPerProc = N / size;
    int remainder   = N % size;
    int startRow, endRow;

    if(rank < remainder) {
        startRow = rank * (rowsPerProc + 1);
        endRow   = startRow + (rowsPerProc + 1);
    } else {
        startRow = remainder * (rowsPerProc + 1) + (rank - remainder) * rowsPerProc;
        endRow   = startRow + rowsPerProc;
    }
    int localRows = endRow - startRow;

    // 分配本地网格(大小 localRows * N)
    std::vector<int> current(localRows*N, EMPTY);
    std::vector<int> nextState(localRows*N, EMPTY);

    // 把 globalGrid 中对应行的数据拷贝到 current
    // 只在 rank=0 时 globalGrid 才是真实的(若我们设计了在 root 收集/广播)
    //   如果你想要效率高一些，可以使用 MPI_Scatterv 实现分发。此处演示简单做法：
    //   - root 进程把 globalGrid 分块发给其它进程
    //   - 或者所有进程先拿到整个 globalGrid，然后各自提取本地部分
    // 下面演示的方法：先 MPI_Bcast 给所有进程，然后各自拷贝
    MPI_Bcast(globalGrid.data(), N*N, MPI_INT, 0, MPI_COMM_WORLD);
    for(int r = 0; r < localRows; r++) {
        for(int c = 0; c < N; c++) {
            current[r*N + c] = globalGrid[(startRow + r)*N + c];
        }
    }

    // 第一步：将 top row 的树点燃(仅在全局 top row)
    // 只需要在 rank=0 的那块对其所包含的 top row 做处理即可
    if(rank == 0) {
        for(int c = 0; c < N; c++) {
            if(current[c] == TREE) {
                current[c] = BURNING; // 这一行是 row=0
            }
        }
    }

    // 记录哪些 cell 在底部行曾经燃烧(判断是否到达底部)
    // 本地底部行是 global row = N-1, 若 endRow = N, 最后一行可在本地
    // 这里我们在最后聚合时再判断，因此需要一个标记
    bool reachedBottomLocal = false;

    // 用于 ghost 交换的缓冲区
    std::vector<int> topRowSend(N, 0), bottomRowSend(N, 0);
    std::vector<int> topRowRecv(N, 0), bottomRowRecv(N, 0);

    int steps = 0;
    while(true) {
        // 各进程先把 current 拷到 nextState，以便逐步更新
        std::copy(current.begin(), current.end(), nextState.begin());

        // ghost cell 交换：把本地 top row 发给上面进程，对方接收为 bottom ghost
        // 以及把本地 bottom row 发给下面进程，对方接收为 top ghost
        // 如果没有上/下相邻进程，则不用发送
        if(localRows > 0) {
            for(int c = 0; c < N; c++) {
                topRowSend[c]    = current[c];                // local top row = row 0
                bottomRowSend[c] = current[(localRows-1)*N + c]; // local bottom row
            }
        }

        // 发送给上
        if(rank > 0) {
            MPI_Send(topRowSend.data(), N, MPI_INT, rank-1, 0, MPI_COMM_WORLD);
        }
        // 接收来自下的 bottom row
        if(rank < size-1) {
            MPI_Recv(bottomRowRecv.data(), N, MPI_INT, rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        // 发送给下
        if(rank < size-1) {
            MPI_Send(bottomRowSend.data(), N, MPI_INT, rank+1, 1, MPI_COMM_WORLD);
        }
        // 接收来自上的 top row
        if(rank > 0) {
            MPI_Recv(topRowRecv.data(), N, MPI_INT, rank-1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // 现在开始更新 nextState
        // 对于每个 cell，如果它是 TREE，检查其四邻中是否有 BURNING
        //   需要注意是否该邻居在 ghost 行
        auto idx = [&](int r, int c){return r*N + c;};

        for(int r = 0; r < localRows; r++){
            for(int c = 0; c < N; c++){
                int state = current[idx(r,c)];
                if(state == TREE) {
                    bool hasBurningNeighbor = false;
                    // 上邻居
                    if(r > 0) {
                        if(current[idx(r-1,c)] == BURNING) hasBurningNeighbor = true;
                    } else {
                        // 看 ghost row(来自 rank-1)
                        if(rank > 0) {
                            if(topRowRecv[c] == BURNING) {
                                hasBurningNeighbor = true;
                            }
                        }
                    }
                    // 下邻居
                    if(r < localRows-1) {
                        if(current[idx(r+1,c)] == BURNING) hasBurningNeighbor = true;
                    } else {
                        // 看 ghost row(来自 rank+1)
                        if(rank < size-1) {
                            if(bottomRowRecv[c] == BURNING) {
                                hasBurningNeighbor = true;
                            }
                        }
                    }
                    // 左邻居
                    if(c > 0 && current[idx(r,c-1)] == BURNING) {
                        hasBurningNeighbor = true;
                    }
                    // 右邻居
                    if(c < N-1 && current[idx(r,c+1)] == BURNING) {
                        hasBurningNeighbor = true;
                    }

                    if(hasBurningNeighbor) {
                        nextState[idx(r,c)] = BURNING;
                    }
                } else if(state == BURNING) {
                    // 燃烧 -> 死
                    nextState[idx(r,c)] = DEAD;
                }
            }
        }

        // 更新 current
        current.swap(nextState);

        // 是否还有燃烧的点？如果全部进程都没有，则结束
        // 同时也检查底部是否被点燃过
        int localBurningCount = 0;
        bool localReachedBottomThisStep = false;
        for(int r = 0; r < localRows; r++){
            for(int c = 0; c < N; c++){
                if(current[idx(r,c)] == BURNING) {
                    localBurningCount++;
                }
                // 判断全局行号
                int globalRow = startRow + r;
                if(globalRow == N-1 && current[idx(r,c)] == BURNING) {
                    localReachedBottomThisStep = true;
                }
            }
        }
        if(localReachedBottomThisStep) {
            reachedBottomLocal = true;
        }

        // 规约得到全局燃烧数
        int globalBurningCount = 0;
        MPI_Allreduce(&localBurningCount, &globalBurningCount, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        steps++;

        if(globalBurningCount == 0) {
            // 已经没有着火点了
            break;
        }
    }

    // 最终规约判断是否到底部
    int reachedBottomInt = reachedBottomLocal ? 1 : 0;
    int globalReachedBottomInt = 0;
    MPI_Allreduce(&reachedBottomInt, &globalReachedBottomInt, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    bool reachedBottom = (globalReachedBottomInt > 0);

    // 返回(总步数, 是否到底)
    return std::make_pair(steps, reachedBottom);
}

int main(int argc, char** argv){
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if(argc < 4) {
        if(rank == 0) {
            std::cerr << "Usage: mpirun -np <nproc> ./forest_fire N p M [optional_input_file]\n";
        }
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    // 解析命令行参数
    int N = std::atoi(argv[1]);
    double p = std::atof(argv[2]);
    int M = std::atoi(argv[3]);

    // 如果有文件名，则尝试读取网格
    bool useFileInput = false;
    std::string inputFile;
    if(argc > 4) {
        useFileInput = true;
        inputFile = argv[4];
    }

    // 我们在 rank=0 进行网格的初始化，然后广播给其它进程
    std::vector<int> globalGrid;
    if(rank == 0) {
        // 如果从文件读取
        if(useFileInput) {
            globalGrid = readGridFromFile(inputFile, N);
        }
            // 否则随机生成
            // 生成时可以重复 M 次，每次可以重新seed
        else {
            // 先生成一个初始网格用作第1次模拟，后面会根据run索引改变seed
            unsigned int seed = (unsigned int)time(NULL);
            globalGrid = generateRandomGrid(N, p, seed);
        }
    } else {
        // 非 root 进程先分配空间
        globalGrid.resize(N*N, 0);
    }

    // 用于累计结果
    double totalTime = 0.0;
    long long totalSteps = 0;
    int fireReachedBottomCount = 0; // 统计有多少次到达底部

    // M 次重复
    for(int run = 0; run < M; run++) {
        // 每次都要保证 globalGrid 是正确的初始状态
        // 若使用文件输入，则所有 run 都用同一个 grid；
        // 若随机生成，则每个 run 都重新生成
        if(!useFileInput) {
            if(rank == 0) {
                // 根据 run 的不同 seed 生成新的随机网格
                // 这样才是真正 M 次独立的初始条件
                unsigned int newSeed = (unsigned int)time(NULL) + run*10000;
                globalGrid = generateRandomGrid(N, p, newSeed);
            }
        }

        MPI_Barrier(MPI_COMM_WORLD); // 同步

        double startTime = MPI_Wtime();
        // 进行一次模拟
        auto result = runSimulationOnce(globalGrid, N, rank, size);
        double endTime = MPI_Wtime();
        double elapsed = endTime - startTime;

        // 每个进程把 steps、 reachedBottom 发到 root 做汇总
        int localSteps = result.first;
        bool localReached = result.second;

        int globalSteps = 0;
        MPI_Reduce(&localSteps, &globalSteps, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);

        int localR = (localReached ? 1 : 0);
        int globalR = 0;
        MPI_Reduce(&localR, &globalR, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);

        double globalElapsed = 0.0;
        MPI_Reduce(&elapsed, &globalElapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

        if(rank == 0) {
            totalTime   += globalElapsed;
            totalSteps  += globalSteps;
            fireReachedBottomCount += globalR;
        }
    }

    // root 进程输出平均结果
    if(rank == 0) {
        double avgTime   = totalTime / M;
        double avgSteps  = (double)totalSteps / M;
        double fracReach = (double)fireReachedBottomCount / M;

        std::cout << "=============================================\n";
        std::cout << "Forest Fire Simulation Results\n";
        std::cout << "  N = " << N << ", p = " << p << ", M = " << M << "\n";
        std::cout << "---------------------------------------------\n";
        std::cout << "  Average steps before fire stops: " << avgSteps << "\n";
        std::cout << "  Fraction of runs that reached bottom: " << fracReach << "\n";
        std::cout << "  Average wall time (max among procs): " << avgTime << " s\n";
        std::cout << "=============================================\n";
    }

    MPI_Finalize();
    return 0;
}
