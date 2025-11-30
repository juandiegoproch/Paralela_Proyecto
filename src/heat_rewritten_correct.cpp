#include <cmath>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <mpi.h>
#include <omp.h>

// Globals
unsigned int grid_x = 20; 
unsigned int grid_y = 20;
int itmax = 2000;
const double dominio_x_max = 1.0;
const double dominio_y_max = 1.0;

// FLOPs per cell per iteration
// Kernel: ((L+R - 2v)*C1 + (U+D - 2v)*C2)*dt + v
// Adds/Subs: 7, Muls: 4 (approx 10-11 depending on compiler optimization)
// We will use 10 as the theoretical baseline.
const int FLOPS_PER_CELL = 10;

int main(int argc, char** argv){
    MPI_Init(&argc, &argv);
    int rank, n_proc;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_proc);

    // Arguments: [GRID_X] [GRID_Y] [ITERATIONS]
    if (argc >= 2) grid_x = std::atoi(argv[1]);
    if (argc >= 3) grid_y = std::atoi(argv[2]);
    if (argc >= 4) itmax  = std::atoi(argv[3]);

    // Math Setup
    double dx = dominio_x_max / (double)grid_x;
    double dy = dominio_y_max / (double)grid_y;
    double dx2 = dx * dx;
    double dy2 = dy * dy;
    double dx2i = 1.0 / dx2;
    double dy2i = 1.0 / dy2;
    double dt = std::min(dx2, dy2) / 4.0; 

    // Domain Decomposition
    int start = (rank * grid_x) / n_proc;
    int end = ((rank + 1) * grid_x) / n_proc;
    if (rank == n_proc - 1) end = grid_x;

    int local_grid_x = end - start;
    int local_grid_y = grid_y;
    int stride = local_grid_x + 2;
    
    std::vector<double> phi((local_grid_x + 2) * (local_grid_y + 2), 0.0);
    std::vector<double> phi_working = phi;

    auto idx = [stride](int x, int y){ return y * stride + x; };

    // Initialization
    #pragma omp parallel for collapse(2)
    for (int x = 0; x < local_grid_x + 2; ++x) {
        for (int y = 0; y < local_grid_y + 2; ++y) {
            if (y == local_grid_y + 1) phi[idx(x,y)] = 1.0;
        }
    }
    std::copy(phi.begin(), phi.end(), phi_working.begin());

    MPI_Datatype column_type;
    MPI_Type_vector(local_grid_y, 1, stride, MPI_DOUBLE, &column_type);
    MPI_Type_commit(&column_type);

    MPI_Barrier(MPI_COMM_WORLD); // Sync before timing
    double start_time = MPI_Wtime();
    
    // --- Main Loop ---
    for (int it = 0; it < itmax; ++it) {
        MPI_Request reqs[4];
        int r = 0;

        // Halo Exchange
        if (rank != 0)          MPI_Irecv(&phi[idx(0, 1)], 1, column_type, rank - 1, 0, MPI_COMM_WORLD, &reqs[r++]);
        if (rank != n_proc - 1) MPI_Irecv(&phi[idx(local_grid_x + 1, 1)], 1, column_type, rank + 1, 0, MPI_COMM_WORLD, &reqs[r++]);
        if (rank != 0)          MPI_Isend(&phi[idx(1, 1)], 1, column_type, rank - 1, 0, MPI_COMM_WORLD, &reqs[r++]);
        if (rank != n_proc - 1) MPI_Isend(&phi[idx(local_grid_x, 1)], 1, column_type, rank + 1, 0, MPI_COMM_WORLD, &reqs[r++]);
        
        if (r > 0) MPI_Waitall(r, reqs, MPI_STATUSES_IGNORE);

        // Compute
        #pragma omp parallel for collapse(2)
        for (int x = 1; x <= local_grid_x; ++x) {
            for (int y = 1; y <= local_grid_y; ++y) {
                double val = phi[idx(x, y)];
                // 10 FLOPs per line below:
                double dphi = ((phi[idx(x+1, y)] + phi[idx(x-1, y)] - 2.0 * val) * dx2i 
                             + (phi[idx(x, y+1)] + phi[idx(x, y-1)] - 2.0 * val) * dy2i) * dt;
                phi_working[idx(x, y)] = val + dphi;
            }
        }
        std::swap(phi, phi_working);
    }
    
    double end_time = MPI_Wtime();
    double elapsed = end_time - start_time;

    // --- Metrics Analysis (Rank 0) ---
    if (rank == 0) {
        // Total Theoretical FLOPs = Grid_Points * Iterations * FLOPs_per_point
        long long total_cells = (long long)grid_x * grid_y;
        long long total_ops   = total_cells * itmax * FLOPS_PER_CELL;
        double gflops_executed = (double)total_ops / 1e9;
        double gflops_per_sec  = gflops_executed / elapsed;

        // CSV Output: NP, GridX, Iterations, Time(s), Total_GFLOPs, GFLOPs/sec
        std::cout << "DATA," << n_proc << "," << grid_x << "," << itmax << "," 
                  << elapsed << "," << gflops_executed << "," << gflops_per_sec << std::endl;
    }

    MPI_Type_free(&column_type);
    MPI_Finalize();
    return 0;
}
