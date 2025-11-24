#!/bin/bash

# Master run script for Heat Equation Solver
# 1. Compiles on Login Node
# 2. Submits to Slurm
# 3. Monitors Output automatically

set -e

# Configuration
NP=${NP:-4}
OMP_THREADS=${OMP_NUM_THREADS:-2}
SLURM_FILE="heat.slurm"
SRC_DIR="src"
BINARY="heat_rewritten"
SOURCE="${SRC_DIR}/heat_rewritten.cpp"
JOB_NAME="heat_solver"

# Modules matching your system
GCC_MODULE="gnu12"
MPI_MODULE="openmpi4"
PMIX_MODULE="pmix"

echo "=================================================="
echo "   HPC Heat Equation Solver - Automated Runner"
echo "=================================================="
echo "Configuration:"
echo "  MPI Processes:      $NP"
echo "  OpenMP Threads:     $OMP_THREADS"
echo "  Partition:          standard"
echo "  Modules:            $GCC_MODULE, $MPI_MODULE"
echo "=================================================="
echo ""

# ---------------------------------------------------------
# 1. Compilation (Login Node)
# ---------------------------------------------------------
echo "[1/3] Compiling on login node..."

# Load modules for compilation
if command -v ml &> /dev/null; then
    ml load "$GCC_MODULE" "$MPI_MODULE" "$PMIX_MODULE" 2>/dev/null || true
elif command -v module &> /dev/null; then
    module load "$GCC_MODULE" "$MPI_MODULE" "$PMIX_MODULE" 2>/dev/null || true
fi

# Compile
if [ ! -f "$SOURCE" ]; then
    echo "Error: Source file $SOURCE not found!"
    exit 1
fi

mpic++ -fopenmp -o "${SRC_DIR}/${BINARY}" "$SOURCE"
echo "      Success! Binary created at ${SRC_DIR}/${BINARY}"
echo ""

# ---------------------------------------------------------
# 2. Job Submission
# ---------------------------------------------------------
echo "[2/3] Submitting Slurm job..."

if [ ! -f "$SLURM_FILE" ]; then
    echo "Error: Slurm file $SLURM_FILE not found!"
    exit 1
fi

# Auto-detect user's default account for standard partition
# Try to get default account, fallback to first available account
DEFAULT_ACCOUNT=$(sacctmgr show user $USER withassoc format=defaultaccount -n 2>/dev/null | head -1 | awk '{print $1}')
if [ -z "$DEFAULT_ACCOUNT" ]; then
    DEFAULT_ACCOUNT=$(sacctmgr show user $USER withassoc format=account -n 2>/dev/null | head -1 | awk '{print $1}')
fi

# Get absolute path to current directory for Slurm
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Submit and extract Job ID
# Add account if we found one (standard partition may require it)
# Use --chdir to ensure job starts in the correct directory
SBATCH_ARGS="--ntasks=$NP --cpus-per-task=$OMP_THREADS --job-name=$JOB_NAME --chdir=$SCRIPT_DIR"
if [ -n "$DEFAULT_ACCOUNT" ]; then
    SBATCH_ARGS="$SBATCH_ARGS --account=$DEFAULT_ACCOUNT"
    echo "      Using account: $DEFAULT_ACCOUNT"
fi

OUTPUT=$(sbatch $SBATCH_ARGS "$SLURM_FILE" 2>&1)
SUBMIT_SUCCESS=$?

if [ $SUBMIT_SUCCESS -ne 0 ]; then
    echo "Error: Job submission failed!"
    echo "$OUTPUT"
    exit 1
fi

# Extract Job ID from output
JOB_ID=$(echo "$OUTPUT" | awk '/Submitted batch job/ {print $4}')

if [[ -z "$JOB_ID" ]] || [[ ! "$JOB_ID" =~ ^[0-9]+$ ]]; then
    echo "Error: Could not extract Job ID from submission output:"
    echo "$OUTPUT"
    exit 1
fi

echo "      Job submitted! ID: $JOB_ID"
echo ""

# ---------------------------------------------------------
# 3. Monitoring
# ---------------------------------------------------------
echo "[3/3] Monitoring job $JOB_ID..."
echo "      Waiting for job to start..."

# Wait for job to start running
while true; do
    STATE=$(squeue -j "$JOB_ID" -h -o %T 2>/dev/null || echo "COMPLETED")
    
    if [ "$STATE" == "RUNNING" ]; then
        echo "      Job is RUNNING."
        break
    elif [ "$STATE" == "COMPLETED" ] || [ "$STATE" == "FAILED" ] || [ "$STATE" == "CANCELLED" ] || [ "$STATE" == "TIMEOUT" ]; then
        echo "      Job finished before monitoring could start (very fast run?)."
        break
    elif [ -z "$STATE" ]; then
        # Job disappeared from queue
        echo "      Job finished."
        break
    fi
    sleep 1
done

# Define output files
OUT_FILE="slurm-${JOB_ID}.out"
ERR_FILE="slurm-${JOB_ID}.err"

# Wait for output file to appear (Slurm can be slightly delayed writing it)
echo "      Waiting for output files..."
MAX_RETRIES=10
count=0
while [ ! -f "$OUT_FILE" ] && [ $count -lt $MAX_RETRIES ]; do
    sleep 1
    ((count++))
    # If job is already done, files might be ready
    if ! squeue -j "$JOB_ID" &>/dev/null; then
        break
    fi
done

echo ""
echo "================ JOB OUTPUT ================"

# Tail the output file if it exists
if [ -f "$OUT_FILE" ]; then
    # Use tail -f --pid to follow until the tail process is killed or file stops growing? 
    # Better: loop while job is running or just cat at the end if short.
    # For robust "live" monitoring:
    tail -f "$OUT_FILE" &
    TAIL_PID=$!
    
    # Wait until job finishes
    while squeue -j "$JOB_ID" &>/dev/null; do
        sleep 2
    done
    
    # Kill tail
    kill $TAIL_PID 2>/dev/null || true
    
    # Show any remaining lines
    echo "--- Final Output ---"
    cat "$OUT_FILE"
else
    echo "Warning: Output file $OUT_FILE not found yet."
fi

echo "============================================"

# Check for errors
if [ -f "$ERR_FILE" ] && [ -s "$ERR_FILE" ]; then
    echo "STDERR content:"
    cat "$ERR_FILE"
    echo "============================================"
fi

echo "Done."
