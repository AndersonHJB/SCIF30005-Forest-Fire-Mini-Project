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

}






















