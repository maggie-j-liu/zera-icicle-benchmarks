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


inline void ntt(scalar_t *input, size_t n, std::vector<scalar_t> &output) {
    for (int i = 0; i < n; i++) {
        output[i] = input[i];
    }
    const auto &omega_br = get_precomputed(n).omega_br;
    // size_t t = n / 2;
    // for (size_t m = 1; m < n; m *= 2, t /= 2) {
    //     cilk_for (size_t i = 0; i < m; i++) {
    //         const size_t j_start = 2 * i * t;
    //         const size_t j_end = j_start + t;
    //         cilk_for (size_t j = j_start; j < j_end; j++) {
    //             const auto tmp = output[j + t] * omega_br[i];
    //             output[j + t] = output[j] - tmp;
    //             output[j] = output[j] + tmp;
    //         }
    //     }
    // }
    size_t log_n = clog2(n);
    for (size_t log_m = 0, log_t = log_n - 1; log_m < log_n; log_m++, log_t--) {
        const size_t t = 1 << log_t;
        cilk_gpu_for (size_t ij = 0; ij < (n / 2); ij++) {
            const size_t i = ij >> log_t;
            const size_t j = (i << (log_t + 1)) + (ij & (t - 1));
            const auto tmp = output[j + t] * omega_br[i];
            output[j + t] = output[j] - tmp;
            output[j] = output[j] + tmp;
        }
    }
}


auto ntt(scalar_t *input, size_t n) -> std::vector<scalar_t> {
    auto output = std::vector<scalar_t>(n);
    ntt(input, n, output);
    return output;
}

void run_benchmark(int log_ntt_size, int trials) {
	std::cout << "\n=== Zera ntt, log size=" << log_ntt_size << " ===" << std::endl;
	int ntt_size = 1 << log_ntt_size;
    auto input = std::make_unique<scalar_t[]>(ntt_size);

	// Initialize NTT domain with fast twiddles (CUDA backend)
    scalar_t basic_root = scalar_t::omega(log_ntt_size);
    auto ntt_init_domain_cfg = default_ntt_init_domain_config();
    ntt_init_domain(basic_root, ntt_init_domain_cfg);

	std::vector<scalar_t> output;

	for (int i = 0; i < 3; i++) {
		output = ntt(input.get(), ntt_size);
	}

	ctimer_t t;
    ctimer_start(&t);
	for (int i = 0; i < trials; i++) {
		output = ntt(input.get(), ntt_size);
	}
	ctimer_stop(&t);
    ctimer_measure(&t);

    long ns = timespec_nsec(t.elapsed);
    double ms = ns / 1000000.0 / trials;

    std::cout << "avg time: "
              << ms
              << " ms" << std::endl;
	
	ntt_release_domain<scalar_t>();
}


int main() {
	int log_sizes[] = {18, 20, 22};
    int trials = 10;

    for (auto s : log_sizes) {
        run_benchmark(s, trials);
    }

	return 0;
}