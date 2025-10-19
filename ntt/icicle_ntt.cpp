#include "icicle/curves/params/bn254.h"
#include "icicle/backend/ntt_config.h"
#include "icicle/ntt.h"
#include <iostream>
#include <memory>
#include <bit>
#include <type_traits>
#include <span>
#include <mutex>
#include <ctimer.h>

using namespace bn254;

int main() {
    icicle_load_backend_from_env_or_default();

    Device device_cpu = {"CPU", 0};
    Device device_gpu = {"CUDA", 0};
    if (icicle_is_device_available("CUDA") != eIcicleError::SUCCESS) {
        std::cout << "CUDA not available, using CPU only" << std::endl;
        device_gpu = device_cpu;
    }

    icicle_set_device(device_gpu);
    int batch_size = 1;
    int log_ntt_size = 22;
    long long ntt_size = 1 << log_ntt_size;
    auto inputs = std::make_unique<scalar_t[]>(batch_size * ntt_size);
    auto outputs = std::make_unique<scalar_t[]>(batch_size * ntt_size);
    scalar_t::rand_host_many(inputs.get(), ntt_size);

    // Initialize NTT domain with fast twiddles (CUDA backend)
    scalar_t basic_root = scalar_t::omega(log_ntt_size);
    auto ntt_init_domain_cfg = default_ntt_init_domain_config();
    ConfigExtension backend_cfg_ext;
    backend_cfg_ext.set(CudaBackendConfig::CUDA_NTT_FAST_TWIDDLES_MODE, true);
    ntt_init_domain_cfg.ext = &backend_cfg_ext;
    ntt_init_domain(basic_root, ntt_init_domain_cfg);

    NTTConfig<scalar_t> config = default_ntt_config<scalar_t>();
    ConfigExtension ntt_cfg_ext;
    config.batch_size = batch_size;
    config.ordering = Ordering::kNR;
    config.are_inputs_on_device = true;
    config.are_outputs_on_device = true;
    config.ext = &ntt_cfg_ext;

    scalar_t* inputs_d;
    scalar_t* outputs_d;
    icicle_malloc((void**)&inputs_d, sizeof(scalar_t) * ntt_size);
    icicle_malloc((void**)&outputs_d, sizeof(scalar_t) * ntt_size);
    icicle_copy(inputs_d, inputs.get(), sizeof(scalar_t) * ntt_size);

    ctimer_t t;
    ctimer_start(&t);
    ntt(inputs_d, ntt_size, NTTDir::kForward, config, outputs_d);
    ctimer_stop(&t);
    ctimer_measure(&t);

    icicle_copy(outputs.get(), outputs_d, sizeof(scalar_t) * ntt_size);

    long ns = timespec_nsec(t.elapsed);
    double ms = ns / 1000000.0;
    std::cout << "OUTPUT" << std::endl;
    std::cout << "time " << ms << " ms" << std::endl;

    std::cout << outputs[0] << std::endl;
    
    icicle_free(inputs_d);
    icicle_free(outputs_d);
    ntt_release_domain<scalar_t>();
}