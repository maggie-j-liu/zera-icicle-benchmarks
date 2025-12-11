// python3 ock++.py -O2 -o zera_vector_add_profile zera_vector_add_profile.cpp -Iinclude -I/home/magpie/icicle-install/icicle/include -I/usr/local/cuda-12.9/include
#include <iostream>
#include <memory>
#include <chrono>
#include <cilk/cilk.h>
#include <ctimer.h>
#include <cuda_profiler_api.h>

#include "icicle/curves/params/bls12_381.h"

using namespace bls12_381; // scalar_t

void vector_add(const scalar_t* a, const scalar_t* b, scalar_t* out, size_t size) {
  [[tapir::target("cuda"), tapir::grain_size(1)]] cilk_for (size_t i = 0; i < size; i++) {
    out[i] = a[i] + b[i];
  }
}

void run(size_t size) {
  std::cout << "\n=== Zera vector_add: size=" << size << " ===" << std::endl;

  auto h_a = std::make_unique<scalar_t[]>(size);
  auto h_b = std::make_unique<scalar_t[]>(size);
  auto h_out = std::make_unique<scalar_t[]>(size);

  scalar_t::rand_host_many(h_a.get(), size);
  scalar_t::rand_host_many(h_b.get(), size);

  // Warm-up
  for (int i = 0; i < 5; i++) {
    vector_add(h_a.get(), h_b.get(), h_out.get(), size);
  }

  cudaProfilerStart();

  vector_add(h_a.get(), h_b.get(), h_out.get(), size);

  cudaProfilerStop();
}

int main() {
//   size_t sizes[] = {1 << 20, 1 << 23, 1 << 27}; // ~1M, 8M, 134M
  size_t sizes[] = {1 << 23}; // ~1M, 8M, 134M

  for (auto s : sizes) {
    run(s);
  }

  return 0;
}
