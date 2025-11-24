#!/bin/bash
#SBATCH --job-name=heat_solver
#SBATCH --output=slurm-%j.out
#SBATCH --error=slurm-%j.err
#SBATCH --ntasks=4                 # Number of MPI processes (can be overridden via sbatch --ntasks)
#SBATCH --cpus-per-task=2          # CPUs per task (for OpenMP threads, can be overridden)
#SBATCH --time=00:10:00            # Wall clock time limit (10 minutes)
#SBATCH --partition=standard       # Partition name
#SBATCH --mem-per-cpu=512M         # Memory per CPU

# Optional: Uncomment and adjust these if needed
#SBATCH --nodes=1                  # Number of nodes
#SBATCH --ntasks-per-node=4        # MPI tasks per node
#SBATCH --account=your_account     # Account/Project name

echo "=========================================="
echo "Job ID: $SLURM_JOB_ID"
echo "Job Name: $SLURM_JOB_NAME"
echo "Node: $SLURM_NODELIST"
echo "Number of tasks: $SLURM_NTASKS"
echo "CPUs per task: $SLURM_CPUS_PER_TASK"
echo "Number of nodes: $SLURM_JOB_NUM_NODES"
echo "=========================================="
echo ""

# Get NP from environment or use SLURM_NTASKS
NP=${NP:-$SLURM_NTASKS}
OMP_THREADS=${OMP_NUM_THREADS:-$SLURM_CPUS_PER_TASK}

echo "Running with:"
echo "  MPI processes: $NP"
echo "  OpenMP threads per process: $OMP_THREADS"
echo ""

# Load modules
module load gnu12 openmpi4 pmix 2>/dev/null || ml load gnu12 openmpi4 pmix 2>/dev/null || true

# Slurm sets SLURM_SUBMIT_DIR to the directory where sbatch was executed
# If --chdir was used in sbatch, we should already be in the right directory
# But we'll use SLURM_SUBMIT_DIR as the authoritative source
if [ -n "$SLURM_SUBMIT_DIR" ]; then
    cd "$SLURM_SUBMIT_DIR"
fi

# Verify we're in the right place and binary exists
echo "Working directory: $(pwd)"
echo "Checking for binary: src/heat_rewritten"

if [ ! -f "src/heat_rewritten" ]; then
    echo "ERROR: Binary not found at: $(pwd)/src/heat_rewritten"
    echo "Current directory contents:"
    ls -la
    echo ""
    echo "Looking for src/ directory:"
    if [ -d "src" ]; then
        echo "src/ directory exists. Contents:"
        ls -la src/
    else
        echo "src/ directory does not exist in $(pwd)"
    fi
    echo ""
    echo "SLURM_SUBMIT_DIR: ${SLURM_SUBMIT_DIR:-not set}"
    echo "Please compile on the login node first using: ./run.sh"
    exit 1
fi

echo "Binary found! Proceeding with execution..."
echo ""

# Run the executable
echo "Starting execution..."
echo ""

cd src
export OMP_NUM_THREADS=$OMP_THREADS

# Use mpirun (OpenMPI without PMI support)
mpirun -np "$NP" ./heat_rewritten

EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo "Job completed successfully!"
else
    echo "Job failed with exit code: $EXIT_CODE"
fi

exit $EXIT_CODE

