#!/bin/bash

# Cross-platform run script for MPI+OpenMP heat equation solver
# Supports macOS and Linux

set -e  # Exit on error

# Default values
NP=${NP:-4}              # Number of MPI processes
OMP_THREADS=${OMP_NUM_THREADS:-2}  # OpenMP threads per process
SRC_DIR="src"
BINARY="heat_rewritten"
SOURCE="${SRC_DIR}/heat_rewritten.cpp"

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
else
    echo "Warning: Unknown OS type: $OSTYPE. Assuming Linux..."
    OS="linux"
fi

echo "Detected OS: $OS"
echo "MPI processes: $NP"
echo "OpenMP threads per process: $OMP_THREADS"
echo ""

# Check if source file exists
if [ ! -f "$SOURCE" ]; then
    echo "Error: Source file not found: $SOURCE"
    exit 1
fi

# Compile with platform-specific flags
echo "Compiling..."
if [ "$OS" == "macos" ]; then
    # macOS (clang) requires special OpenMP flags
    if command -v brew &> /dev/null; then
        LIBOMP_PREFIX=$(brew --prefix libomp 2>/dev/null || echo "")
        if [ -n "$LIBOMP_PREFIX" ]; then
            # Homebrew libomp is installed
            mpic++ -Xpreprocessor -fopenmp -lomp \
                   -I"${LIBOMP_PREFIX}/include" \
                   -L"${LIBOMP_PREFIX}/lib" \
                   -o "${SRC_DIR}/${BINARY}" \
                   "$SOURCE"
        else
            # Try without explicit paths (might work if libomp is in system paths)
            mpic++ -Xpreprocessor -fopenmp -lomp \
                   -o "${SRC_DIR}/${BINARY}" \
                   "$SOURCE" || {
                echo "Error: OpenMP not found. Install with: brew install libomp"
                exit 1
            }
        fi
    else
        # No Homebrew, try system paths
        mpic++ -Xpreprocessor -fopenmp -lomp \
               -o "${SRC_DIR}/${BINARY}" \
               "$SOURCE" || {
            echo "Error: OpenMP not found. Install libomp or use Homebrew."
            exit 1
        }
    fi
else
    # Linux (usually GCC) - standard OpenMP flag
    mpic++ -fopenmp -o "${SRC_DIR}/${BINARY}" "$SOURCE"
fi

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

echo "Compilation successful!"
echo ""

# Run the program
echo "Running..."
cd "$SRC_DIR"
export OMP_NUM_THREADS=$OMP_THREADS
mpirun -np "$NP" ./"$BINARY"

