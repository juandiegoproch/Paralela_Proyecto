# Parallelization Strategy: Hybrid MPI + OpenMP

This heat equation solver uses a **hybrid parallelization approach** combining MPI (Message Passing Interface) and OpenMP to achieve both inter-process and intra-process parallelism.

## Overview

The code solves the 2D heat equation using a finite difference method on an 80×80 grid. The parallelization strategy uses:

1. **MPI**: Distributes the grid across multiple processes (coarse-grained parallelism)
2. **OpenMP**: Parallelizes loops within each process (fine-grained parallelism)

## Two-Level Parallelization

### Level 1: MPI - Domain Decomposition (Strip Partitioning)

**What it does:**
- Divides the 80×80 grid into **vertical strips** along the X-axis
- Each MPI process owns a contiguous set of columns
- Example with 4 processes: Process 0 gets columns 0-19, Process 1 gets 20-39, etc.

**Key Code Sections:**

```cpp
// Lines 31-34: Domain partitioning
int start = (rank * grid_x) / n_proc;
int end = ((rank + 1) * grid_x) / n_proc;
if (rank == n_proc - 1) end = grid_x;
```

**Communication Pattern:**
- Each process needs boundary data from its neighbors (ghost columns)
- Lines 78-96: Non-blocking MPI sends/receives exchange boundary columns
- Uses `MPI_Type_vector` to efficiently send entire columns

**Benefits:**
- Scales across multiple nodes/machines
- Each process works on independent data
- Communication only at boundaries (minimal overhead)

### Level 2: OpenMP - Loop Parallelization

**What it does:**
- Within each MPI process, OpenMP threads parallelize the computation loops
- Uses `collapse(2)` to parallelize nested loops efficiently

**Parallelized Loops:**

1. **Initialization Loop** (lines 50-62):
   ```cpp
   #pragma omp parallel for collapse(2)
   for (int x = 0; x < local_grid_x + 2; ++x) {
       for (int y = 0; y < local_grid_y + 2; ++y) {
           // Initialize grid values
       }
   }
   ```

2. **Main Computation Loop** (lines 101-115):
   ```cpp
   #pragma omp parallel for collapse(2)
   for (int x = 1; x <= local_grid_x; ++x) {
       for (int y = 1; y <= local_grid_y; ++y) {
           // Stencil computation (5-point stencil)
       }
   }
   ```
   This is the **most critical** parallelization - it runs 20,000 iterations!

3. **Result Copying Loop** (lines 121-126):
   ```cpp
   #pragma omp parallel for collapse(2)
   for (int y = 1; y <= local_grid_y; ++y) {
       for (int x = 1; x <= local_grid_x; ++x) {
           // Copy results
       }
   }
   ```

**Why `collapse(2)`?**
- Combines the two nested loops into a single parallel loop
- Better load balancing across threads
- More iterations to distribute = better parallel efficiency

## Total Parallelism

**Example Configuration:**
- 4 MPI processes × 2 OpenMP threads = **8-way parallelism**

**Work Distribution:**
```
Grid (80×80):
├─ MPI Process 0 (columns 0-19)  → OpenMP threads work on 20×80 subgrid
├─ MPI Process 1 (columns 20-39) → OpenMP threads work on 20×80 subgrid
├─ MPI Process 2 (columns 40-59) → OpenMP threads work on 20×80 subgrid
└─ MPI Process 3 (columns 60-79)  → OpenMP threads work on 20×80 subgrid
```

## Communication Pattern

**Each Iteration:**
1. MPI processes exchange boundary columns (ghost cells)
2. OpenMP threads compute interior points in parallel
3. Swap read/write buffers
4. Repeat for 20,000 iterations

**Communication Overhead:**
- Only boundary columns are exchanged (minimal data transfer)
- Non-blocking MPI calls allow overlap with computation
- OpenMP has zero communication overhead (shared memory)

## Performance Benefits

1. **Scalability**: MPI allows scaling across multiple machines
2. **Efficiency**: OpenMP reduces overhead within each process
3. **Load Balancing**: `collapse(2)` ensures even work distribution
4. **Memory Locality**: Each process works on contiguous memory regions

## Thread Safety

- **MPI calls**: All MPI operations are in sequential sections (safe)
- **OpenMP loops**: Each iteration writes to unique memory locations
- **No race conditions**: Read from `phi`, write to `phi_working`, then swap

## Usage

See `run.sh` for compilation and execution. Set:
- `NP`: Number of MPI processes
- `OMP_NUM_THREADS`: OpenMP threads per process

Example:
```bash
NP=4 OMP_NUM_THREADS=2 ./run.sh
```

