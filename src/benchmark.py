import subprocess
import csv

# --- Configuration ---
# Strong Scaling: Keep Grid Fixed, Increase Processors
# Weak Scaling: Increase Grid and Processors (Optional, focusing on Strong here)
processors_list = [1, 2, 4, 8, 16, 32]
grid_sizes = [256, 512, 1024, 2048, 4096]
iterations = 1000

# Output file
csv_filename = "benchmark_results_final.csv"

def run_experiment(np, grid_x, grid_y, iters):
    cmd = ["mpirun", "-np", str(np), "./heat_rewritten_correct", str(grid_x), str(grid_y), str(iters)]
    
    try:
        # Run command and capture output
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        
        # Parse output for the line starting with "DATA"
        for line in result.stdout.splitlines():
            if line.startswith("DATA,"):
                # Remove "DATA," and split by comma
                return line.strip().split(",")[1:]
    except subprocess.CalledProcessError as e:
        print(f"Error running NP={np}: {e}")
        return None

# --- Main Execution ---
print(f"Starting Benchmark... Saving to {csv_filename}")

with open(csv_filename, 'w', newline='') as csvfile:
    writer = csv.writer(csvfile)
    # Write Header
    writer.writerow(["NP", "GridX", "Iterations", "Time_Sec", "Total_GFLOPs", "GFLOPs_Sec"])

    for size in grid_sizes:
        print(f"\n--- Testing Grid Size: {size}x{size} ---")
        
        # Base time for speedup calculation (Time at NP=1)
        t_serial = 0.0
        
        for np in processors_list:
            print(f"Running with NP={np}...", end=" ", flush=True)
            
            data = run_experiment(np, size, size, iterations)
            
            if data:
                # data format: [NP, GridX, Iterations, Time, TotalGFLOPs, GFLOPs_Sec]
                writer.writerow(data)
                
                time_taken = float(data[3])
                flops_sec = float(data[5])
                
                if np == 1:
                    t_serial = time_taken
                    speedup = 1.0
                else:
                    speedup = t_serial / time_taken if time_taken > 0 else 0
                
                print(f"Done. Time={time_taken:.4f}s | Speedup={speedup:.2f}x | GFLOPs/s={flops_sec:.2f}")
            else:
                print("Failed.")

print(f"\nBenchmark finished. Data saved to {csv_filename}")
