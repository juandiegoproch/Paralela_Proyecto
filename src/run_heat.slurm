#!/bin/bash
#SBATCH --job-name=hybrid_heat      # Job name
#SBATCH --output=result_%j.out      # Standard output log (%j = job ID)
#SBATCH --error=error_%j.err        # Standard error log
#SBATCH --nodes=2                   # Number of nodes to request
#SBATCH --ntasks-per-node=2         # MPI processes per node
#SBATCH --cpus-per-task=8           # OpenMP threads per MPI process
#SBATCH --time=00:10:00             # Time limit (HH:MM:SS)
#SBATCH --partition=standard        # Partition name (Change this to your cluster's partition)

# --- Environment Setup ---
# Purge existing modules to start clean
module purge

# Load the modules available on your specific cluster
# Based on your 'module avail' output:
module load gnu12/12.4.0
module load openmpi4/4.1.6

# --- Hybrid Configuration ---
# Set OMP_NUM_THREADS to match the CPUs requested via Slurm
export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK

# Processor binding to prevent threads from jumping cores
export OMP_PROC_BIND=true
export OMP_PLACES=cores

echo "Starting Hybrid Job"
echo "Nodes: $SLURM_JOB_NUM_NODES"
echo "MPI Ranks per Node: $SLURM_NTASKS_PER_NODE"
echo "OpenMP Threads per Rank: $OMP_NUM_THREADS"
echo "------------------------------------------------"

# --- Execution ---
# Note: Some clusters use 'srun' instead of 'mpirun'. 
# If mpirun fails, try: srun ./corrected

mpirun ./original

echo "------------------------------------------------"
echo "Job Finished"
