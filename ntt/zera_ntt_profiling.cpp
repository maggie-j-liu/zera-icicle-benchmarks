// #include "ntt.hpp"
// #include "field.hpp"
// #include "timer.hpp"

// #include <mutex>
// #include <random>
// #include <unordered_map>

// #include <catch2/benchmark/catch_benchmark.hpp>
// #include <catch2/catch_test_macros.hpp>
// #include <fmt/ranges.h>
// #include <spdlog/spdlog.h>

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
#include "cilk.h"
#include <nvtx3/nvtx3.hpp>

using namespace bn254;

#define cilk_gpu_for [[tapir::target("cuda"), tapir::grain_size(1)]] cilk_for

struct NTTPrecomputed {
    std::vector<scalar_t> omega_br;
};

template <std::unsigned_integral T> constexpr auto clog2(T x) -> T {
    return x == 0 ? 0 : std::bit_width(x - 1);
}

scalar_t twiddle(uint64_t n) {
    // n must be a power of two and > 0
    // get_root_of_unity returns the root for the requested size (uses precomputed tables)
    scalar_t rou;
    get_root_of_unity<scalar_t>(n, &rou);
    return rou;
}

inline static auto bit_reverse(size_t x, size_t log_n) -> size_t {
    size_t res = 0;
    for (size_t i = 0; i < log_n; i++) {
        res = (res << 1) | (x & 1);
        x >>= 1;
    }
    return res;
}

auto get_precomputed(size_t n) -> const NTTPrecomputed & {
    static std::mutex cache_mutex;
    static std::vector<std::optional<NTTPrecomputed>> cache(4096);

    std::lock_guard<std::mutex> lock(cache_mutex);
    const size_t log_n = clog2(n);
    if (const auto &p = cache[log_n]; p) {
        return *p;
    }

    NTTPrecomputed p;
    const auto omega = twiddle(n);
    p.omega_br.resize(n / 2);
    cilk_for (size_t i = 0; i < n / 2; i++) {
        p.omega_br[i] = omega.pow(bit_reverse(i, log_n - 1));
    }
    return (cache[log_n] = std::move(p)).value();
}


inline void ntt(scalar_t *input, size_t n, scalar_t* output) {
	
	auto precomputed_range = nvtx3::start_range("get_precomputed");
	const auto &omega_br = get_precomputed(n).omega_br;
	nvtx3::end_range(precomputed_range);
	// {
	// 	nvtx3::scoped_range c{"copy input"};
	// 	for (int i = 0; i < n; i++) {
	// 		output[i] = input[i];
	// 	}
	// }
	size_t log_n = clog2(n);
	for (size_t log_m = 0, log_t = log_n - 1; log_m < log_n; log_m++, log_t--) {
		nvtx3::scoped_range d{"iteration"};
		const size_t t = 1 << log_t;
		cilk_gpu_for (size_t ij = 0; ij < (n / 2); ij++) {
			const size_t i = ij >> log_t;
			const size_t j = (i << (log_t + 1)) + (ij & (t - 1));
			const auto tmp = (log_m == 0 ? input[j + t] : output[j + t]) * omega_br[i];
			output[j + t] = (log_m == 0 ? input[j] : output[j]) - tmp;
			output[j] = (log_m == 0 ? input[j] : output[j]) + tmp;
		}
	}
}

void ntt(scalar_t *input, scalar_t *output, size_t n, int batch_size) {
	cilk_for (int i = 0; i < batch_size; i++) {
		const auto row = input + n * i;
    	ntt(row, n, output + n * i);
	}
}

void run_benchmark(int log_ntt_size, int batch_size) {
	std::cout << "\n=== Zera ntt, log size=" << log_ntt_size << ", batch size=" << batch_size << " ===" << std::endl;
	int ntt_size = 1 << log_ntt_size;
    auto input = std::make_unique<scalar_t[]>(ntt_size * batch_size);
	scalar_t::rand_host_many(input.get(), ntt_size * batch_size);

	// Initialize NTT domain with fast twiddles (CUDA backend)
    scalar_t basic_root = scalar_t::omega(log_ntt_size);
    auto ntt_init_domain_cfg = default_ntt_init_domain_config();
    ntt_init_domain(basic_root, ntt_init_domain_cfg);

    auto output = std::make_unique<scalar_t[]>(ntt_size * batch_size);
	ntt(input.get(), output.get(), ntt_size, batch_size);

	ctimer_t t;
    ctimer_start(&t);
	ntt(input.get(), output.get(), ntt_size, batch_size);
	ctimer_stop(&t);
    ctimer_measure(&t);

    long ns = timespec_nsec(t.elapsed);
    double ms = ns / 1000000.0;

    std::cout << "avg time: "
              << ms
              << " ms" << std::endl;
	
	ntt_release_domain<scalar_t>();
}


int main() {
	int batch_sizes[] = {1};
	int log_sizes[] = {16};

    for (auto batch_size : batch_sizes) {
    	for (auto s : log_sizes) {
        	run_benchmark(s, batch_size);
    	}
	}

	return 0;
}