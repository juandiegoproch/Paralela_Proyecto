#include <cmath>
#include <vector>
#include <cstring>
#include <iostream>
#include <mpi.h>

int main(int argc, char** argv){
    // Parametros del problema
    const unsigned int grid_x = 80;
    const unsigned int grid_y = 80;

    const double dominio_x_max = 1.0;
    const double dominio_y_max = 1.0;

    const int itmax = 20000;

    // Parametros calculados
    double dx = dominio_x_max / grid_x;
    double dy = dominio_y_max / grid_y;
    double dt = std::min(dx*dx, dy*dy) / 4.0; // criterio de estabilidad

    double dx2 = dx*dx, dy2 = dy*dy;

    // Inicializacion de MPI
    MPI_Init(&argc, &argv);
    int rank, n_proc;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_proc);

    // particion del dominio en X (intervalos [start,end[)
    int start = (rank * grid_x) / n_proc;
    int end = ((rank + 1) * grid_x) / n_proc;
    if (rank == n_proc - 1) end = grid_x;

    // Tamaño local en X e Y
    int local_grid_x = end - start; // número de columnas reales que manejo
    int local_grid_y = grid_y;      // particion solo en x

    // +2 por halos (ghost columns/rows)
    // NOTE: We choose a layout: idx(x,y) = y*(local_grid_x+2) + x
    // so x is the fastest (inner index). This means a vertical column (fixed x)
    // has stride (local_grid_x+2) between successive y elements -> needs an MPI vector.
    int stride = local_grid_x + 2;
    size_t phi_size = (size_t)(local_grid_x + 2) * (size_t)(local_grid_y + 2);

    std::vector<double> phi(phi_size);
    std::vector<double> phi_working(phi_size);

    auto idx = [stride](int x, int y){ return y * stride + x; };

    // condiciones iniciales y de frontera
    for (int x = 0; x < local_grid_x + 2; ++x) {
        for (int y = 0; y < local_grid_y + 2; ++y) {
            if (x == 0 || x == local_grid_x + 1) {
                // ramp in y at left/right global boundaries for this local block
                phi[idx(x,y)] = y * dy;
            } else if (y == local_grid_y + 1) {
                phi[idx(x,y)] = 1.0; // ones at the top boundary
            } else {
                phi[idx(x,y)] = 0.0;
            }
        }
    }
    std::copy(phi.begin(), phi.end(), phi_working.begin());

    // Create an MPI derived datatype for a column (vertical slice):
    // a column is local_grid_y elements with stride 'stride' (distance between y rows).
    MPI_Datatype column_type;
    MPI_Type_vector(local_grid_y, 1, stride, MPI_DOUBLE, &column_type);
    MPI_Type_commit(&column_type);

    // main loop
    for (int it = 0; it < itmax; ++it) {
        // Intercambio de halos (columns)
        MPI_Request reqs[4];
        int r = 0;
        int tag = 0;

        // post non-blocking receives into ghost columns
        if (rank != 0) {
            // receive left ghost column into x=0, starting at y=1
            MPI_Irecv(&phi[idx(0, 1)], 1, column_type, rank - 1, tag, MPI_COMM_WORLD, &reqs[r++]);
        }
        if (rank != n_proc - 1) {
            // receive right ghost column into x=local_grid_x+1, starting at y=1
            MPI_Irecv(&phi[idx(local_grid_x + 1, 1)], 1, column_type, rank + 1, tag, MPI_COMM_WORLD, &reqs[r++]);
        }

        // post non-blocking sends of local boundary columns
        if (rank != 0) {
            // send our leftmost interior column x=1 to rank-1
            MPI_Isend(&phi[idx(1, 1)], 1, column_type, rank - 1, tag, MPI_COMM_WORLD, &reqs[r++]);
        }
        if (rank != n_proc - 1) {
            // send our rightmost interior column x=local_grid_x to rank+1
            MPI_Isend(&phi[idx(local_grid_x, 1)], 1, column_type, rank + 1, tag, MPI_COMM_WORLD, &reqs[r++]);
        }

        if (r) MPI_Waitall(r, reqs, MPI_STATUSES_IGNORE);

        // update interior points (use local indices x=1..local_grid_x, y=1..local_grid_y)
        for (int x = 1; x <= local_grid_x; ++x) {
            for (int y = 1; y <= local_grid_y; ++y) {
                double upper_neighbour = phi[idx(x, y - 1)];
                double lower_neighbour = phi[idx(x, y + 1)];
                double left_neighbour  = phi[idx(x - 1, y)];
                double right_neighbour = phi[idx(x + 1, y)];
                double this_cell = phi[idx(x, y)];

                double dphi = ((right_neighbour + left_neighbour - 2.0 * this_cell) * dx2
                              + (lower_neighbour + upper_neighbour - 2.0 * this_cell) * dy2)
                              * dt;
                phi_working[idx(x, y)] = this_cell + dphi;
            }
        }

        std::swap(phi, phi_working);
    }
    // copy without the halos
    std::vector<double> partial_result(local_grid_x * local_grid_y);
    for (int y = 1; y <= local_grid_y; ++y) {
        for (int x = 1; x <= local_grid_x; ++x) {
            partial_result[(y - 1) * local_grid_x + (x - 1)] = phi[idx(x, y)];
        }
    }
    
    // gather all partial results into rank 0
    std::vector<double> result;
    if (rank == 0) {
        result.resize(grid_x * grid_y);
    }
    
    // calculate send counts and displacements for gatherv
    std::vector<int> recvcounts(n_proc);
    std::vector<int> displs(n_proc);
    for (int i = 0; i < n_proc; i++) {
        int istart = (i * grid_x) / n_proc;
        int iend = ((i + 1) * grid_x) / n_proc;
        if (i == n_proc - 1) iend = grid_x;
        recvcounts[i] = (iend - istart) * grid_y;
        displs[i] = istart * grid_y;
    }

    MPI_Gatherv(partial_result.data(), local_grid_x * local_grid_y, MPI_DOUBLE,
                result.data(), recvcounts.data(), displs.data(), MPI_DOUBLE,
                0, MPI_COMM_WORLD);

    if (rank == 0) {
        for (int y = 0; y < grid_y; ++y) {
            for (int x = 0; x < grid_x; ++x) {
                std::cout << result[y * grid_x + x];
                if (x + 1 < grid_x) std::cout << " ";
            }
            std::cout << "\n";
        }
    }

    MPI_Type_free(&column_type);
    MPI_Finalize();
    return 0;
}