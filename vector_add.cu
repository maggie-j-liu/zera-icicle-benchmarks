
#include <iostream>
#include <chrono>
#include <random>
#include <memory>
#include <cstdlib>

#include <cuda_profiler_api.h>
#include <cuda_runtime.h>

__global__ void vector_add_kernel(const uint64_t* a, const uint64_t* b, uint64_t* out, size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;

    for (size_t i = idx; i < n; i += stride) {
        out[i] = a[i] + b[i];
    }
}

void cuda_vector_add(const uint64_t* h_a, const uint64_t* h_b, uint64_t* h_out, size_t n, int gridSize, int blockSize) {
    uint64_t *d_a, *d_b, *d_out;

    // Allocate device memory
    cudaMalloc(&d_a, n * sizeof(uint64_t));
    cudaMalloc(&d_b, n * sizeof(uint64_t));
    cudaMalloc(&d_out, n * sizeof(uint64_t));

    // Copy inputs
    cudaMemcpy(d_a, h_a, n * sizeof(uint64_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, h_b, n * sizeof(uint64_t), cudaMemcpyHostToDevice);

    cudaDeviceSynchronize();

    for (int i = 0; i < 5; i++) {
        vector_add_kernel<<<gridSize, blockSize>>>(d_a, d_b, d_out, n);
    }

    cudaProfilerStart();

    // Launch kernel
    vector_add_kernel<<<gridSize, blockSize>>>(d_a, d_b, d_out, n);

    cudaProfilerStop();

    // Sync and copy result back
    cudaDeviceSynchronize();
    cudaMemcpy(h_out, d_out, n * sizeof(uint64_t), cudaMemcpyDeviceToHost);

    // Free device memory
    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_out);
}

int main(int argc, char* argv[]) {
    size_t n = 1 << 23; // ~8 million elements

    // Defaults
    int blockSize = 256;
    int gridSize = (n + blockSize - 1) / blockSize;

    if (argc >= 3) {
        blockSize = std::atoi(argv[1]);
        gridSize  = std::atoi(argv[2]);
        if (blockSize <= 0 || gridSize <= 0) {
            std::cerr << "Invalid block/grid size arguments.\n";
            return 1;
        }
    }

    std::cout << "Running with blockSize=" << blockSize
              << ", gridSize=" << gridSize
              << ", totalThreads=" << (long long)blockSize * gridSize
              << ", n=" << n << std::endl;

    auto h_a   = std::make_unique<uint64_t[]>(n);
    auto h_b   = std::make_unique<uint64_t[]>(n);
    auto h_out = std::make_unique<uint64_t[]>(n);

    std::mt19937_64 eng(std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<uint64_t> dist;

    for (size_t i = 0; i < n; i++) {
        h_a[i] = dist(eng);
        h_b[i] = dist(eng);
    }

    cuda_vector_add(h_a.get(), h_b.get(), h_out.get(), n, gridSize, blockSize);

    return 0;
}