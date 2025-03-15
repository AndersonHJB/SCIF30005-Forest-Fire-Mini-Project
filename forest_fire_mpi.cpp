#include <mpi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <random>
#include <sstream>

// 状态常量：EMPTY=0, TREE=1, BURNING=2, BURNT=-1（或 3 亦可）
static const int EMPTY = 0;
static const int TREE = 1;
static const int BURNING = 2;
static const int BURNT = -1;

// 使用结构体，用于收集某次模拟的结果
struct SimulationResult {
    int steps;
    bool reachedBottom;
    double timeUsed;
};

// 局部网格里计算某个格点在下一步的状态
int nextState(int currentState, int up, int down, int left, int right) {
    if (currentState == TREE) {
        if (up == BURNING || down == BURNING || left == BURNING || right == BURNING) {
            return BURNING;
        } else {
            return TREE;
        }
    } else if (currentState == BURNING) {
        return BURNT;
    } else {
        return currentState;
    }
}

bool readGridFromFile(const std::string &filename, std::vector<int> &grid, int N) {
    std::ifstream  fin(filename.c_str());
    if (!fin.is_open()) {
        return false;
    }
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            int val;
            fin >> val;
            grid[i * N + j]  = val;
        }
    }
    fin.close();
    return false;
}

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

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

}


























