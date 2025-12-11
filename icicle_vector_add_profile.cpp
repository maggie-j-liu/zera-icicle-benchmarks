// clang++ -O2 -o icicle_vector_add_profile icicle_vector_add_profile.cpp -Iinclude -I/home/magpie/icicle-install/icicle/include -I/usr/local/cuda-12.9/include -L/home/magpie/icicle-install/icicle/lib -L/usr/local/cuda-12.9/lib64 -licicle_device -licicle_field_bls12_381 -licicle_curve_bls12_381 -lcudart -Wl,-rpath,/home/magpie/icicle-install/icicle/lib
#include <iostream>
#include <memory>
#include <chrono>

#include "icicle/runtime.h"
#include "icicle/vec_ops.h"
#include "icicle/curves/params/bls12_381.h"

#include <ctimer.h>
#include <cuda_profiler_api.h>

using namespace bls12_381; // scalar_t

void run(Device device, size_t size) {
  std::cout << "\n=== ICICLE CUDA vector_add, size=" << size << " ===" << std::endl;

  icicle_set_device(device);

  // Host allocations
  auto h_a = std::make_unique<scalar_t[]>(size);
  auto h_b = std::make_unique<scalar_t[]>(size);
  auto h_out = std::make_unique<scalar_t[]>(size);

  scalar_t::rand_host_many(h_a.get(), size);
  scalar_t::rand_host_many(h_b.get(), size);

  // Device allocations
  scalar_t *d_a, *d_b, *d_out;
  icicle_malloc((void**)&d_a, size * sizeof(scalar_t));
  icicle_malloc((void**)&d_b, size * sizeof(scalar_t));
  icicle_malloc((void**)&d_out, size * sizeof(scalar_t));

  icicle_copy(d_a, h_a.get(), size * sizeof(scalar_t));
  icicle_copy(d_b, h_b.get(), size * sizeof(scalar_t));

  VecOpsConfig config;
  config.is_a_on_device = true;
  config.is_b_on_device = true;
  config.is_result_on_device = true;
  config.is_async = false;

  // Warm-up
  for (int i = 0; i < 5; i++) {
    vector_add<scalar_t>(d_a, d_b, size, config, d_out);
  }
  icicle_device_synchronize();

  cudaProfilerStart();
  
  vector_add<scalar_t>(d_a, d_b, size, config, d_out);
  icicle_device_synchronize();

  cudaProfilerStop();


  icicle_free(d_a);
  icicle_free(d_b);
  icicle_free(d_out);
}

int main() {
  icicle_load_backend_from_env_or_default();

  Device device_cpu = {"CPU", 0};
  Device device_gpu = {"CUDA", 0};
  if (icicle_is_device_available("CUDA") != eIcicleError::SUCCESS) {
    std::cout << "CUDA not available, using CPU only" << std::endl;
    device_gpu = device_cpu;
  }

  size_t sizes[] = {1 << 23}; // ~1M, 8M, 134M
//   size_t sizes[] = {1 << 20, 1 << 23, 1 << 27}; // ~1M, 8M, 134M

  for (auto s : sizes) {
    run(device_gpu, s);
  }

  return 0;
}
