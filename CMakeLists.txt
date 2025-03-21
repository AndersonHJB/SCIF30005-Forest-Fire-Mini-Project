cmake_minimum_required(VERSION 3.30)
project(untitled12)

set(CMAKE_CXX_STANDARD 20)
# 查找 MPI
find_package(MPI REQUIRED)

# 设置 include 和 link 目录
include_directories(${MPI_INCLUDE_PATH})
link_directories(${MPI_LIBRARY_PATH})

# 添加可执行文件
add_executable(untitled12 forest_fire_mpi.cpp
)

# 链接 MPI
target_link_libraries(untitled12 ${MPI_CXX_LIBRARIES})
