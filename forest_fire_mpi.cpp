#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <string>
#include <mpi.h>
#include <chrono>
#include <cmath>

enum CellState {
    EMPTY = 0,
    TREE = 1,
    BURNING = 2,
    DEAD = 3
};

class ForestFire {
private:
    int N;
    double p;
    int rank;
    int num_procs;
    int local_rows;
    int global_start_row;

    std::vector<std::vector<int>> grid;
    std::vector<std::vector<int>> next_grid;
    std::mt19937 rng;


    std::vector<int> send_top;
    std::vector<int> send_bottom;
    std::vector<int> recv_top;
    std::vector<int> recv_bottom;

public:
    ForestFire(int N, double p, int seed, int rank, int num_procs)
            : N(N), p(p), rank(rank), num_procs(num_procs), rng(seed + rank) {

        local_rows = N / num_procs;
        if (rank < N % num_procs) {
            local_rows++;
        }


        global_start_row = 0;
        for (int i = 0; i < rank; i++) {
            global_start_row += N / num_procs;
            if (i < N % num_procs) {
                global_start_row++;
            }
        }


        grid.resize(local_rows + 2, std::vector<int>(N, EMPTY));
        next_grid.resize(local_rows + 2, std::vector<int>(N, EMPTY));

        send_top.resize(N, EMPTY);
        send_bottom.resize(N, EMPTY);
        recv_top.resize(N, EMPTY);
        recv_bottom.resize(N, EMPTY);
    }

    // Generate a random grid
    void generateRandomGrid() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        // Fill the grid with trees based on probability p
        for (int i = 1; i <= local_rows; i++) {
            for (int j = 0; j < N; j++) {
                if (dist(rng) < p) {
                    grid[i][j] = TREE;
                } else {
                    grid[i][j] = EMPTY;
                }
            }
        }

        if (global_start_row == 0) {
            for (int j = 0; j < N; j++) {
                if (grid[1][j] == TREE) {
                    grid[1][j] = BURNING;
                }
            }
        }
    }


    void readGridFromFile(const std::string& filename) {
        std::vector<std::vector<int>> full_grid;

        if (rank == 0) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Error opening file: " << filename << std::endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            // Read the full grid
            std::string line;
            while (std::getline(file, line)) {
                std::vector<int> row;
                for (char c : line) {
                    if (c == '0' || c == '1') {
                        row.push_back(c - '0');
                    }
                }
                if (!row.empty()) {
                    full_grid.push_back(row);
                }
            }
            file.close();

            // Check if the grid is square
            N = full_grid.size();
            for (const auto& row : full_grid) {
                if (row.size() != N) {
                    std::cerr << "Error: Grid is not square" << std::endl;
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
            }
        }

        // Broadcast N to all processes
        MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

        // Recalculate rows per process
        local_rows = N / num_procs;
        if (rank < N % num_procs) {
            local_rows++;
        }

        // Recalculate global starting row
        global_start_row = 0;
        for (int i = 0; i < rank; i++) {
            global_start_row += N / num_procs;
            if (i < N % num_procs) {
                global_start_row++;
            }
        }

        // Resize grids and buffers
        grid.resize(local_rows + 2, std::vector<int>(N, EMPTY));
        next_grid.resize(local_rows + 2, std::vector<int>(N, EMPTY));
        send_top.resize(N, EMPTY);
        send_bottom.resize(N, EMPTY);
        recv_top.resize(N, EMPTY);
        recv_bottom.resize(N, EMPTY);

        // Distribute grid to processes
        if (rank == 0) {
            // Master keeps its own part
            for (int i = 0; i < local_rows; i++) {
                for (int j = 0; j < N; j++) {
                    grid[i+1][j] = full_grid[i][j];
                }
            }

            // Send parts to other processes
            int current_row = local_rows;
            for (int dest = 1; dest < num_procs; dest++) {
                int dest_rows = N / num_procs;
                if (dest < N % num_procs) {
                    dest_rows++;
                }

                for (int i = 0; i < dest_rows; i++) {
                    MPI_Send(full_grid[current_row + i].data(), N, MPI_INT, dest, 0, MPI_COMM_WORLD);
                }
                current_row += dest_rows;
            }
        } else {
            // Receive grid part from master
            for (int i = 0; i < local_rows; i++) {
                MPI_Recv(grid[i+1].data(), N, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }

        // Set fire to the top row if this process has it
        if (global_start_row == 0) {
            for (int j = 0; j < N; j++) {
                if (grid[1][j] == TREE) {
                    grid[1][j] = BURNING;
                }
            }
        }
    }

    // Exchange ghost cells with neighboring processes
    void exchangeBoundaries() {
        MPI_Request requests[4];
        MPI_Status statuses[4];
        int req_count = 0;

        // Prepare data to send
        for (int j = 0; j < N; j++) {
            send_top[j] = grid[1][j];
            send_bottom[j] = grid[local_rows][j];
        }

        // Send/Receive top boundary
        if (rank > 0) {
            MPI_Isend(send_top.data(), N, MPI_INT, rank-1, 0, MPI_COMM_WORLD, &requests[req_count++]);
            MPI_Irecv(recv_top.data(), N, MPI_INT, rank-1, 1, MPI_COMM_WORLD, &requests[req_count++]);
        }

        // Send/Receive bottom boundary
        if (rank < num_procs - 1) {
            MPI_Isend(send_bottom.data(), N, MPI_INT, rank+1, 1, MPI_COMM_WORLD, &requests[req_count++]);
            MPI_Irecv(recv_bottom.data(), N, MPI_INT, rank+1, 0, MPI_COMM_WORLD, &requests[req_count++]);
        }

        // Wait for all communications to complete
        MPI_Waitall(req_count, requests, statuses);

        // Update ghost cells
        if (rank > 0) {
            for (int j = 0; j < N; j++) {
                grid[0][j] = recv_top[j];
            }
        }

        if (rank < num_procs - 1) {
            for (int j = 0; j < N; j++) {
                grid[local_rows+1][j] = recv_bottom[j];
            }
        }
    }

    // Run a single time step of the simulation
    bool step() {
        bool still_burning = false;

        // Exchange boundaries before the step
        exchangeBoundaries();

        // Apply rules to each cell
        for (int i = 1; i <= local_rows; i++) {
            for (int j = 0; j < N; j++) {
                switch (grid[i][j]) {
                    case EMPTY:
                        next_grid[i][j] = EMPTY;
                        break;
                    case TREE:
                    {  // Added scope brackets to fix the variable initialization issue
                        // Check if any neighbors are burning
                        bool neighbor_burning = false;
                        if (i > 0 && grid[i-1][j] == BURNING) neighbor_burning = true;
                        if (i < local_rows+1 && grid[i+1][j] == BURNING) neighbor_burning = true;
                        if (j > 0 && grid[i][j-1] == BURNING) neighbor_burning = true;
                        if (j < N-1 && grid[i][j+1] == BURNING) neighbor_burning = true;

                        if (neighbor_burning) {
                            next_grid[i][j] = BURNING;
                            still_burning = true;
                        } else {
                            next_grid[i][j] = TREE;
                        }
                    }
                        break;
                    case BURNING:
                        next_grid[i][j] = DEAD;
                        break;
                    case DEAD:
                        next_grid[i][j] = DEAD;
                        break;
                }
            }
        }


        std::swap(grid, next_grid);


        int global_still_burning = 0;
        MPI_Allreduce(&still_burning, &global_still_burning, 1, MPI_INT, MPI_LOR, MPI_COMM_WORLD);

        return global_still_burning;
    }


    bool fireReachedBottom() {
        bool local_reached_bottom = false;

        if (global_start_row + local_rows == N) {
            for (int j = 0; j < N; j++) {
                if (grid[local_rows][j] == BURNING || grid[local_rows][j] == DEAD) {
                    local_reached_bottom = true;
                    break;
                }
            }
        }

        int global_reached_bottom = 0;
        MPI_Allreduce(&local_reached_bottom, &global_reached_bottom, 1, MPI_INT, MPI_LOR, MPI_COMM_WORLD);

        return global_reached_bottom;
    }

    void printGrid() {
        std::vector<std::vector<int>> full_grid;

        if (rank == 0) {
            full_grid.resize(N, std::vector<int>(N, EMPTY));

            for (int i = 0; i < local_rows; i++) {
                for (int j = 0; j < N; j++) {
                    full_grid[i][j] = grid[i+1][j];
                }
            }

            int current_row = local_rows;
            for (int src = 1; src < num_procs; src++) {
                int src_rows = N / num_procs;
                if (src < N % num_procs) {
                    src_rows++;
                }

                std::vector<int> temp_row(N);
                for (int i = 0; i < src_rows; i++) {
                    MPI_Recv(temp_row.data(), N, MPI_INT, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    for (int j = 0; j < N; j++) {
                        full_grid[current_row + i][j] = temp_row[j];
                    }
                }
                current_row += src_rows;
            }

            std::cout << "Grid:" << std::endl;
            for (int i = 0; i < N; i++) {
                for (int j = 0; j < N; j++) {
                    char c;
                    switch (full_grid[i][j]) {
                        case EMPTY: c = '.'; break;
                        case TREE: c = 'T'; break;
                        case BURNING: c = 'B'; break;
                        case DEAD: c = 'D'; break;
                        default: c = '?'; break;
                    }
                    std::cout << c << " ";
                }
                std::cout << std::endl;
            }
        } else {
            // Send grid part to master
            for (int i = 0; i < local_rows; i++) {
                MPI_Send(grid[i+1].data(), N, MPI_INT, 0, 0, MPI_COMM_WORLD);
            }
        }
    }
};


void runSimulation(int N, double p, int seed, int& steps, bool& reached_bottom, double& elapsed_time) {

}


void runMultipleSimulations(int N, double p, int M, double& avg_steps, double& fire_reached_bottom_ratio, double& avg_time) {

}


void runConvergenceStudy(int N, const std::vector<double>& p_values, const std::vector<int>& M_values) {

}


void runPerformanceAnalysis(const std::vector<int>& N_values, double p, int M) {

}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Check command line arguments
    if (argc < 2 && rank == 0) {
        std::cout << "Usage: " << argv[0] << " [mode] [parameters]" << std::endl;
        std::cout << "Modes:" << std::endl;
        std::cout << "  1: Run single simulation (N p [file])" << std::endl;
        std::cout << "  2: Run multiple simulations (N p M)" << std::endl;
        std::cout << "  3: Run convergence study" << std::endl;
        std::cout << "  4: Run performance analysis" << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }


    return 0;
}