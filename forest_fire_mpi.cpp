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
    int N;                       // Grid size
    double p;                    // Probability of tree
    int rank;                    // MPI rank
    int num_procs;               // Number of MPI processes
    int local_rows;              // Number of rows handled by this process
    int global_start_row;        // Starting row in global grid

    std::vector<std::vector<int>> grid;        // Current state
    std::vector<std::vector<int>> next_grid;   // Next state
    std::mt19937 rng;                          // Random number generator

    // MPI communication buffers
    std::vector<int> send_top;   // Buffer to send to process above
    std::vector<int> send_bottom;// Buffer to send to process below
    std::vector<int> recv_top;   // Buffer to receive from process above
    std::vector<int> recv_bottom;// Buffer to receive from process below

public:
    ForestFire(int N, double p, int seed, int rank, int num_procs)
            : N(N), p(p), rank(rank), num_procs(num_procs), rng(seed + rank) {

        // Calculate number of rows per process
        local_rows = N / num_procs;
        if (rank < N % num_procs) {
            local_rows++;
        }

        // Calculate global starting row for this process
        global_start_row = 0;
        for (int i = 0; i < rank; i++) {
            global_start_row += N / num_procs;
            if (i < N % num_procs) {
                global_start_row++;
            }
        }

        // Initialize grid with extra rows for ghost cells
        grid.resize(local_rows + 2, std::vector<int>(N, EMPTY));
        next_grid.resize(local_rows + 2, std::vector<int>(N, EMPTY));

        // Initialize communication buffers
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

        // Set fire to the top row if this process has it
        if (global_start_row == 0) {
            for (int j = 0; j < N; j++) {
                if (grid[1][j] == TREE) {
                    grid[1][j] = BURNING;
                }
            }
        }
    }

    // Read grid from file (only master process reads, then distributes)
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

        // Swap grids
        std::swap(grid, next_grid);

        // Check if any process still has burning trees
        int global_still_burning = 0;
        MPI_Allreduce(&still_burning, &global_still_burning, 1, MPI_INT, MPI_LOR, MPI_COMM_WORLD);

        return global_still_burning;
    }

    // Check if fire reached the bottom row
    bool fireReachedBottom() {
        bool local_reached_bottom = false;

        // If this process has the bottom row
        if (global_start_row + local_rows == N) {
            for (int j = 0; j < N; j++) {
                if (grid[local_rows][j] == BURNING || grid[local_rows][j] == DEAD) {
                    local_reached_bottom = true;
                    break;
                }
            }
        }

        // Check across all processes
        int global_reached_bottom = 0;
        MPI_Allreduce(&local_reached_bottom, &global_reached_bottom, 1, MPI_INT, MPI_LOR, MPI_COMM_WORLD);

        return global_reached_bottom;
    }

    // Print the grid (for debugging)
    void printGrid() {
        std::vector<std::vector<int>> full_grid;

        // Gather all parts to the master process
        if (rank == 0) {
            full_grid.resize(N, std::vector<int>(N, EMPTY));

            // Copy master's part
            for (int i = 0; i < local_rows; i++) {
                for (int j = 0; j < N; j++) {
                    full_grid[i][j] = grid[i+1][j];
                }
            }

            // Receive parts from other processes
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

            // Print the grid
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

// Run a single simulation
void runSimulation(int N, double p, int seed, int& steps, bool& reached_bottom, double& elapsed_time) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Start timer
    auto start_time = std::chrono::high_resolution_clock::now();

    // Initialize forest
    ForestFire forest(N, p, seed, rank, size);
    forest.generateRandomGrid();

    // Run simulation until no more trees are burning
    steps = 0;
    bool still_burning = true;
    while (still_burning) {
        still_burning = forest.step();
        steps++;
    }

    // Check if fire reached bottom
    reached_bottom = forest.fireReachedBottom();

    // Stop timer
    auto end_time = std::chrono::high_resolution_clock::now();
    elapsed_time = std::chrono::duration<double>(end_time - start_time).count();
}

// Run multiple simulations and average results
void runMultipleSimulations(int N, double p, int M, double& avg_steps, double& fire_reached_bottom_ratio, double& avg_time) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int total_steps = 0;
    int total_reached_bottom = 0;
    double total_time = 0.0;

    // Each process performs simulations
    for (int i = 0; i < M; i++) {
        int steps;
        bool reached_bottom;
        double elapsed_time;

        // Use a different seed for each run
        int seed = i * size + rank;
        runSimulation(N, p, seed, steps, reached_bottom, elapsed_time);

        total_steps += steps;
        if (reached_bottom) total_reached_bottom++;
        total_time += elapsed_time;
    }

    // Gather results
    int global_total_steps, global_total_reached_bottom;
    double global_total_time;
    MPI_Reduce(&total_steps, &global_total_steps, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&total_reached_bottom, &global_total_reached_bottom, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&total_time, &global_total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    // Calculate averages
    if (rank == 0) {
        avg_steps = static_cast<double>(global_total_steps) / (M * size);
        fire_reached_bottom_ratio = static_cast<double>(global_total_reached_bottom) / (M * size);
        avg_time = global_total_time / (M * size);
    }
}

// Run convergence study for different probabilities and M values
void runConvergenceStudy(int N, const std::vector<double>& p_values, const std::vector<int>& M_values) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        std::cout << "Convergence Study (N = " << N << ")" << std::endl;
        std::cout << "p,M,Avg_Steps,Fire_Reached_Bottom,Avg_Time" << std::endl;
    }

    for (double p : p_values) {
        for (int M : M_values) {
            double avg_steps, fire_reached_bottom_ratio, avg_time;
            runMultipleSimulations(N, p, M, avg_steps, fire_reached_bottom_ratio, avg_time);

            if (rank == 0) {
                std::cout << p << "," << M << "," << avg_steps << ","
                          << fire_reached_bottom_ratio << "," << avg_time << std::endl;
            }
        }
    }
}

// Run performance analysis for different grid sizes
void runPerformanceAnalysis(const std::vector<int>& N_values, double p, int M) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        std::cout << "Performance Analysis (p = " << p << ", M = " << M << ")" << std::endl;
        std::cout << "N,Processes,Avg_Steps,Fire_Reached_Bottom,Avg_Time,Speedup" << std::endl;
    }

    for (int N : N_values) {
        double avg_steps, fire_reached_bottom_ratio, avg_time;
        runMultipleSimulations(N, p, M, avg_steps, fire_reached_bottom_ratio, avg_time);

        // Calculate reference time (for speedup calculation)
        double ref_time = 0.0;
        if (size > 1) {
            MPI_Bcast(&avg_time, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            ref_time = avg_time * size;  // Approximation of sequential time
        } else {
            ref_time = avg_time;
        }

        if (rank == 0) {
            double speedup = ref_time / avg_time;
            std::cout << N << "," << size << "," << avg_steps << ","
                      << fire_reached_bottom_ratio << "," << avg_time << "," << speedup << std::endl;
        }
    }
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

    int mode = 1;
    if (argc >= 2) {
        mode = std::stoi(argv[1]);
    }

    switch (mode) {
        case 1: {  // Single simulation
            if (argc < 4 && rank == 0) {
                std::cout << "Usage: " << argv[0] << " 1 N p [file]" << std::endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            int N = std::stoi(argv[2]);
            double p = std::stod(argv[3]);

            ForestFire forest(N, p, 42, rank, size);

            if (argc >= 5) {
                // Read grid from file
                forest.readGridFromFile(argv[4]);
            } else {
                // Generate random grid
                forest.generateRandomGrid();
            }

            // Run simulation until no more trees are burning
            int steps = 0;
            bool still_burning = true;

            auto start_time = std::chrono::high_resolution_clock::now();

            while (still_burning) {
                still_burning = forest.step();
                steps++;
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            double elapsed_time = std::chrono::duration<double>(end_time - start_time).count();

            // Check if fire reached bottom
            bool reached_bottom = forest.fireReachedBottom();

            if (rank == 0) {
                std::cout << "Simulation completed in " << steps << " steps" << std::endl;
                std::cout << "Fire reached bottom: " << (reached_bottom ? "Yes" : "No") << std::endl;
                std::cout << "Time taken: " << elapsed_time << " seconds" << std::endl;
            }

            break;
        }

        case 2: {  // Multiple simulations
            if (argc < 5 && rank == 0) {
                std::cout << "Usage: " << argv[0] << " 2 N p M" << std::endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            int N = std::stoi(argv[2]);
            double p = std::stod(argv[3]);
            int M = std::stoi(argv[4]);

            double avg_steps, fire_reached_bottom_ratio, avg_time;
            runMultipleSimulations(N, p, M, avg_steps, fire_reached_bottom_ratio, avg_time);

            if (rank == 0) {
                std::cout << "Average steps: " << avg_steps << std::endl;
                std::cout << "Fire reached bottom ratio: " << fire_reached_bottom_ratio << std::endl;
                std::cout << "Average time: " << avg_time << " seconds" << std::endl;
            }

            break;
        }

        case 3: {  // Convergence study
            std::vector<double> p_values = {0.3, 0.4, 0.5, 0.55, 0.6, 0.65, 0.7};
            std::vector<int> M_values = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
            int N = 100;  // Fixed grid size

            runConvergenceStudy(N, p_values, M_values);
            break;
        }

        case 4: {  // Performance analysis
            std::vector<int> N_values = {50, 100, 500};
            double p = 0.6;
            int M = 50;

            runPerformanceAnalysis(N_values, p, M);
            break;
        }

        default:
            if (rank == 0) {
                std::cout << "Invalid mode: " << mode << std::endl;
            }
            MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Finalize();
    return 0;
}