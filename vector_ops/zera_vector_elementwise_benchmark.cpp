#include <iostream>
#include <memory>
#include <chrono>
#include <cilk/cilk.h>
#include <ctimer.h>

#include "icicle/curves/params/bls12_381.h"

using namespace bls12_381; // scalar_t

enum class OpType { Add, Sub, Mul, Div };

std::string op_name(OpType op) {
  switch (op) {
    case OpType::Add: return "vector_add";
    case OpType::Sub: return "vector_sub";
    case OpType::Mul: return "vector_mul";
    case OpType::Div: return "vector_div";
    default: return "unknown";
  }
}

void vector_add(const scalar_t* a, const scalar_t* b, size_t size, scalar_t* out) {
  [[tapir::target("cuda"), tapir::grain_size(1)]] cilk_for (size_t i = 0; i < size; i++) {
    out[i] = a[i] + b[i];
  }
}

void vector_sub(const scalar_t* a, const scalar_t* b, size_t size, scalar_t* out) {
  [[tapir::target("cuda"), tapir::grain_size(1)]] cilk_for (size_t i = 0; i < size; i++) {
    out[i] = a[i] - b[i];
  }
}

void vector_mul(const scalar_t* a, const scalar_t* b, size_t size, scalar_t* out) {
  [[tapir::target("cuda"), tapir::grain_size(1)]] cilk_for (size_t i = 0; i < size; i++) {
    out[i] = a[i] * b[i];
  }
}

void vector_div(const scalar_t* a, const scalar_t* b, size_t size, scalar_t* out) {
  return;
}

void run_op(OpType op, scalar_t* a, scalar_t* b, size_t n, scalar_t* out) {
  switch (op) {
    case OpType::Add:
      vector_add(a, b, n, out);
      break;
    case OpType::Sub:
      vector_sub(a, b, n, out);
      break;
    case OpType::Mul:
      vector_mul(a, b, n, out);
      break;
    case OpType::Div:
      vector_div(a, b, n, out);
      break;
  }
}

void run_benchmark(size_t size, int trials, OpType op) {
  std::cout << "\n=== Zera " << op_name(op) << ": size=" << size << " ===" << std::endl;

  auto h_a = std::make_unique<scalar_t[]>(size);
  auto h_b = std::make_unique<scalar_t[]>(size);
  auto h_out = std::make_unique<scalar_t[]>(size);

  scalar_t::rand_host_many(h_a.get(), size);
  scalar_t::rand_host_many(h_b.get(), size);

  // Warm-up
  for (int i = 0; i < 3; i++) {
	run_op(op, h_a.get(), h_b.get(), size, h_out.get());
  }

  // Timed runs
  ctimer_t t;
  ctimer_start(&t);
//   auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < trials; i++) {
	run_op(op, h_a.get(), h_b.get(), size, h_out.get());
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

  std::vector<OpType> ops = {OpType::Add, OpType::Sub, OpType::Mul};

  for (auto op : ops) {
    for (auto s : sizes) {
      run_benchmark(s, trials, op);
    }
  }

  return 0;
}
