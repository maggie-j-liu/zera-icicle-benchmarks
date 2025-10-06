#include <iostream>

#include "icicle/runtime.h"
#include "icicle/msm.h"
#include "icicle/curves/params/bls12_377.h"
using namespace bls12_377;

void run_benchmark(Device device, size_t msm_size, int trials) {
  std::cout << "\n=== ICICLE CUDA msm, size=" << msm_size << " ===" << std::endl;
  icicle_set_device(device);

  int batch_size = 1;
  int N = batch_size * msm_size;
  std::cout << "Batch size: " << batch_size << std::endl;
  std::cout << "MSM size: " << msm_size << std::endl;

  std::cout << "Generating random inputs on-host" << std::endl;
  auto scalars = std::make_unique<scalar_t[]>(N);
  auto points = std::make_unique<affine_t[]>(N);
  projective_t result;
  scalar_t::rand_host_many(scalars.get(), N);
  projective_t::rand_host_many(points.get(), N);

  auto config = default_msm_config();
  config.batch_size = batch_size;
  config.are_results_on_device = true;
  config.are_scalars_on_device = true;
  config.are_points_on_device = true;

  std::cout << "Copying inputs to-device" << std::endl;
  scalar_t* scalars_d;
  affine_t* points_d;
  projective_t* result_d;

  icicle_malloc((void**)&scalars_d, sizeof(scalar_t) * N);
  icicle_malloc((void**)&points_d, sizeof(affine_t) * N);
  icicle_malloc((void**)&result_d, sizeof(projective_t));
  icicle_copy(scalars_d, scalars.get(), sizeof(scalar_t) * N);
  icicle_copy(points_d, points.get(), sizeof(affine_t) * N);

  std::cout << "Running MSM kernel with on-device inputs" << std::endl;
  // Execute the MSM kernel
  msm(scalars_d, points_d, msm_size, config, result_d);

  // Copy the result back to the host
  icicle_copy(&result, result_d, sizeof(projective_t));
  // Print the result
  std::cout << result.to_affine() << std::endl;
  // Free the device memory
  icicle_free(scalars_d);
  icicle_free(points_d);
  icicle_free(result_d);
}

int main() {
  icicle_load_backend_from_env_or_default();

  Device device_cpu = {"CPU", 0};
  Device device_gpu = {"CUDA", 0};
  if (icicle_is_device_available("CUDA") != eIcicleError::SUCCESS) {
    std::cout << "CUDA not available, using CPU only" << std::endl;
    device_gpu = device_cpu;
  }

  size_t sizes[] = {1 << 20}; // ~1M, 8M, 134M
  int trials = 10;

  for (auto s : sizes) {
    run_benchmark(device_gpu, s, trials);
  }

  return 0;
}