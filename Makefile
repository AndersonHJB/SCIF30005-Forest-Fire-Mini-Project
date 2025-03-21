# 编译器
CXX = mpicxx
# 编译选项：C++11标准、优化级别3、显示所有警告
CXXFLAGS = -std=c++11 -O3 -Wall
# 目标可执行文件
TARGET = forest_fire_mpi

# 编译规则
$(TARGET): forest_fire_mpi.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) forest_fire_mpi.cpp

# 清理规则
clean:
	rm -f $(TARGET)