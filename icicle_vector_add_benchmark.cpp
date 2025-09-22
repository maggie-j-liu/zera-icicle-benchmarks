#include <iostream>
#include <memory>
#include <chrono>

#include "icicle/runtime.h"
#include "icicle/vec_ops.h"
#include "icicle/curves/params/bls12_381.h"

#include <ctimer.h>

using namespace bls12_381; // scalar_t

void run_benchmark(Device device, size_t size, int trials) {
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
  for (int i = 0; i < 3; i++) {
    vector_add<scalar_t>(d_a, d_b, size, config, d_out);
  }

  // Timed runs
  ctimer_t t;
  ctimer_start(&t);

//   auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < trials; i++) {
    vector_add<scalar_t>(d_a, d_b, size, config, d_out);
  }
  icicle_device_synchronize();
//   auto end = std::chrono::high_resolution_clock::now();
  ctimer_stop(&t);
  ctimer_measure(&t);

  long ns = timespec_nsec(t.elapsed);
  double ms = ns / 1000000.0 / trials;

//   double ms = std::chrono::duration<double, std::milli>(end - start).count() / trials;
  double elems_per_sec = size / (ms / 1000.0);
  double gbps = (size * sizeof(scalar_t)) / (ms * 1e6);

  std::cout << "avg time: " << ms << " ms, "
            << elems_per_sec / 1e6 << " Melems/s, "
            << gbps << " GB/s" << std::endl;

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

  size_t sizes[] = {1 << 20, 1 << 23, 1 << 27}; // ~1M, 8M, 134M
  int trials = 10;

  for (auto s : sizes) {
    run_benchmark(device_gpu, s, trials);
  }

  return 0;
}
