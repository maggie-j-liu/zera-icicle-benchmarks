// python3 ock++.py -O2 -o zera_vector_add_profile_int zera_vector_add_profile_int.cpp -Iinclude -I/home/magpie/icicle-install/icicle/include -I/usr/local/cuda-12.9/include
#include <iostream>
#include <memory>
#include <chrono>
#include <cilk/cilk.h>
#include <ctimer.h>
#include <cuda_profiler_api.h>
#include <random>


void vector_add(const uint64_t* a, const uint64_t* b, uint64_t* out, size_t size) {
  [[tapir::target("cuda")]] cilk_for (size_t i = 0; i < size; i++) {
    out[i] = a[i] + b[i];
  }
}

void run(size_t size) {
  std::cout << "\n=== Zera vector_add: size=" << size << " ===" << std::endl;

  auto h_a = std::make_unique<uint64_t[]>(size);
  auto h_b = std::make_unique<uint64_t[]>(size);
  auto h_out = std::make_unique<uint64_t[]>(size);

  std::mt19937_64 eng(std::chrono::system_clock::now().time_since_epoch().count());
  std::uniform_int_distribution<uint64_t> dist;

  for (int i = 0; i < size; i++) {
	h_a[i] = dist(eng);
	h_b[i] = dist(eng);
  }

  // Warm-up
  for (int i = 0; i < 5; i++) {
    vector_add(h_a.get(), h_b.get(), h_out.get(), size);
  }

  cudaProfilerStart();

  vector_add(h_a.get(), h_b.get(), h_out.get(), size);

  cudaProfilerStop();

  std::cout << h_out[2] << std::endl;
}

int main() {
//   size_t sizes[] = {1 << 20, 1 << 23, 1 << 27}; // ~1M, 8M, 134M
  size_t sizes[] = {1 << 23}; // ~1M, 8M, 134M

  for (auto s : sizes) {
    run(s);
  }

  return 0;
}
