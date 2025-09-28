
// clang++ -O2 -o vector_add_icicle vector_add_icicle.cu -Iinclude -I/home/magpie/icicle-install/icicle/include -I/usr/local/cuda-12.9/include -L/usr/local/cuda-12.9/lib64 -lcudart -lcuda --cuda-path=/usr/local/cuda-12.9 --cuda-gpu-arch=sm_90 -L/home/magpie/icicle-install/icicle/lib -licicle_device -licicle_field_bls12_381 -licicle_curve_bls12_381 -Wl,-rpath,/home/magpie/icicle-install/icicle/lib/
#include <iostream>

#include "icicle/runtime.h"
#include "icicle/vec_ops.h"
#include "icicle/curves/params/bls12_381.h"

#include <cuda_profiler_api.h>
#include <cuda_runtime.h>

using namespace bls12_381; 

__global__ void vector_add_kernel(const scalar_t* a, const scalar_t* b, scalar_t* out, size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = a[idx] + b[idx]; // scalar_t defines operator+
    }
}

void cuda_vector_add(const scalar_t* h_a, const scalar_t* h_b, scalar_t* h_out, size_t n) {
    scalar_t *d_a, *d_b, *d_out;

    // Allocate device memory
    cudaMalloc(&d_a, n * sizeof(scalar_t));
    cudaMalloc(&d_b, n * sizeof(scalar_t));
    cudaMalloc(&d_out, n * sizeof(scalar_t));

    // Copy inputs
    cudaMemcpy(d_a, h_a, n * sizeof(scalar_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, h_b, n * sizeof(scalar_t), cudaMemcpyHostToDevice);

    // Kernel launch config
    int blockSize = 256;
    int gridSize = (n + blockSize - 1) / blockSize;

    cudaProfilerStart();

    // Launch kernel
    vector_add_kernel<<<gridSize, blockSize>>>(d_a, d_b, d_out, n);

    cudaProfilerStop();

    // Sync and copy result back
    cudaDeviceSynchronize();
    cudaMemcpy(h_out, d_out, n * sizeof(scalar_t), cudaMemcpyDeviceToHost);

    // Free device memory
    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_out);
}

int main() {
    icicle_load_backend_from_env_or_default();

    Device device_cpu = {"CPU", 0};
    Device device_gpu = {"CUDA", 0};
    if (icicle_is_device_available("CUDA") != eIcicleError::SUCCESS) {
      std::cout << "CUDA not available, using CPU only" << std::endl;
      device_gpu = device_cpu;
    }
    
    size_t n = 1 << 23; // ~1 million elements
    auto h_a = std::make_unique<scalar_t[]>(n);
    auto h_b = std::make_unique<scalar_t[]>(n);
    auto h_out = std::make_unique<scalar_t[]>(n);

    // Fill with random BN254 scalars
    scalar_t::rand_host_many(h_a.get(), n);
    scalar_t::rand_host_many(h_b.get(), n);

    // Run kernel
    cuda_vector_add(h_a.get(), h_b.get(), h_out.get(), n);

    return 0;
}