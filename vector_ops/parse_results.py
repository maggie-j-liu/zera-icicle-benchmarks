import re
import matplotlib.pyplot as plt

# Path to the benchmark output file
benchmark_file = "output.txt"

# Read the file
with open(benchmark_file, "r") as f:
    benchmark_text = f.read()

# Regex to capture operation, size, and avg time
pattern = re.compile(r'===\s*(.*?)\s*:\s*size=(\d+)\s*===\s+avg time:\s*([\d.]+) ms', re.MULTILINE)

# Parse benchmarks
benchmarks = {}
for match in pattern.finditer(benchmark_text):
    lib_op, size, avg_time = match.groups()
    size = int(size)
    avg_time = float(avg_time)
    print(lib_op, size, avg_time)
    
    
    # Determine library and operation
    if "ICICLE" in lib_op:
        lib = "ICICLE"
        op = lib_op.split()[-1]
    else:
        lib = "Zera"
        op = lib_op.split()[-1]
    print(lib, op)
    benchmarks.setdefault(op, {}).setdefault(lib, {})[size] = avg_time / size

# Compute percent slowness of Zera relative to ICICLE
print("Percent slowness of Zera vs ICICLE:")
for op, data in benchmarks.items():
    print(f"\nOperation: {op}", data)
    icicle_times = data.get("ICICLE", {})
    zera_times = data.get("Zera", {})
    for size in sorted(zera_times.keys()):
        if size in icicle_times:
            slowness = 100 * (zera_times[size] - icicle_times[size]) / icicle_times[size]
            print(f"  Size {size}: Zera is {slowness:.2f}% slower than ICICLE")

# Create one plot per operation
for op, data in benchmarks.items():
    sizes = sorted(data.get("ICICLE", {}).keys())
    icicle_times = []
    zera_times = []
    plt.figure(figsize=(8, 5))

    if "ICICLE" in data:
        icicle_times = [data["ICICLE"].get(s, None) for s in sizes]
        plt.plot(sizes, icicle_times, marker='o', label='ICICLE')
    if "Zera" in data:
        zera_times = [data["Zera"].get(s, None) for s in sizes]
        plt.plot(sizes, zera_times, marker='x', label='Zera')

    
    plt.xlabel("Vector size")
    plt.ylabel("Average time per element (ms)")
    plt.title(f"Benchmark: {op}")
    plt.legend()
    plt.grid(True, which="both", ls="--")
    plt.tight_layout()
    
    # Save each figure separately
    filename = f"benchmark_{op}.png"
    plt.savefig(filename)
    print(f"Saved plot: {filename}")
    plt.close()