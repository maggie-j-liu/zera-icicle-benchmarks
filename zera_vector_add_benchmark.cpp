#include <iostream>
#include <memory>
#include <chrono>
#include <cilk/cilk.h>
#include <ctimer.h>

#include "icicle/curves/params/bls12_381.h"

using namespace bls12_381; // scalar_t

void vector_add(const scalar_t* a, const scalar_t* b, scalar_t* out, size_t size) {
  [[tapir::target("cuda")]] cilk_for (size_t i = 0; i < size; i++) {
    out[i] = a[i] + b[i];
  }
}

void run_benchmark(size_t size, int trials) {
  std::cout << "\n=== Zera vector_add: size=" << size << " ===" << std::endl;

  auto h_a = std::make_unique<scalar_t[]>(size);
  auto h_b = std::make_unique<scalar_t[]>(size);
  auto h_out = std::make_unique<scalar_t[]>(size);

  scalar_t::rand_host_many(h_a.get(), size);
  scalar_t::rand_host_many(h_b.get(), size);

  // Warm-up
  for (int i = 0; i < 3; i++) {
    vector_add(h_a.get(), h_b.get(), h_out.get(), size);
  }

  // Timed runs
  ctimer_t t;
  ctimer_start(&t);
//   auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < trials; i++) {
    vector_add(h_a.get(), h_b.get(), h_out.get(), size);
  }
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
}

int main() {
  size_t sizes[] = {1 << 20, 1 << 23, 1 << 27}; // ~1M, 8M, 134M
  int trials = 10;

  for (auto s : sizes) {
    run_benchmark(s, trials);
  }

  return 0;
}
