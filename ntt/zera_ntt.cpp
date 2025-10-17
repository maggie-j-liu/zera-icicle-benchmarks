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

using namespace bn254;

struct NTTPrecomputed {
    std::vector<scalar_t> omega_br;
};

template <std::unsigned_integral T> constexpr auto clog2(T x) -> T {
    return x == 0 ? 0 : std::bit_width(x - 1);
}

void twiddle(uint64_t n, scalar_t* rou) {
    // n must be a power of two and > 0
    // get_root_of_unity returns the root for the requested size (uses precomputed tables)
    get_root_of_unity<scalar_t>(n, rou);
}

inline static auto bit_reverse(size_t x, size_t log_n) -> size_t {
    size_t res = 0;
    for (size_t i = 0; i < log_n; i++) {
        res = (res << 1) | (x & 1);
        x >>= 1;
    }
    return res;
}

auto ntt_naive(scalar_t *input, uint64_t n) -> std::vector<scalar_t> {
    // EXPECT(is_power_of_two(input.size()));
    std::vector<scalar_t> output(n);

    scalar_t omega;
	twiddle(n, &omega);
    const size_t log_n = clog2(n);

    for (size_t i = 0; i < n; ++i) {
        scalar_t sum = scalar_t::zero();
        for (size_t j = 0; j < n; ++j) {
            sum = sum + input[j] * omega.pow(j * i);
        }
        output[bit_reverse(i, log_n)] = sum;
    }

    return output;
}

// constexpr DEVICE_HOST static auto twiddle(size_t n) -> BN254 {
// 	constexpr Word Q[N_WORDS] = {
//         4891460686036598785ULL, 2896914383306846353ULL,
//         13281191951274694749ULL, 3486998266802970665ULL};
// 	BN254 x{GENERATOR}, y{1};
// 	const size_t log_n = clog2(n);
// 	for (size_t i = 0; i < N_WORDS; i++) {
// 		Word b = Q[i];
// 		size_t iters = N_BITS;
// 		if (i == 0) {
// 			b = (b - 1) >> log_n;
// 			iters -= log_n;
// 		}
// 		for (size_t j = 0; j < iters; j++) {
// 			if (b & 1) {
// 				y *= x;
// 			}
// 			x *= x;
// 			b >>= 1;
// 		}
// 	}
// 	return y;
// }

int main() {
    int batch_size = 1;
    int log_ntt_size = 4;
    int ntt_size = 1 << log_ntt_size;
    auto input = std::make_unique<scalar_t[]>(batch_size * ntt_size);
    auto output = std::make_unique<scalar_t[]>(batch_size * ntt_size);
    scalar_t::rand_host_many(input.get(), ntt_size);

    // Initialize NTT domain with fast twiddles (CUDA backend)
    scalar_t basic_root = scalar_t::omega(log_ntt_size);
    auto ntt_init_domain_cfg = default_ntt_init_domain_config();
    ConfigExtension backend_cfg_ext;
    // backend_cfg_ext.set(CudaBackendConfig::CUDA_NTT_FAST_TWIDDLES_MODE, true);
    ntt_init_domain_cfg.ext = &backend_cfg_ext;
    ntt_init_domain(basic_root, ntt_init_domain_cfg);

    // scalar_t rou;
    // twiddle(ntt_size, &rou);
	const auto output_naive = ntt_naive(input.get(), ntt_size);

	std::cout << "OUTPUT NAIVE" << std::endl;
	for (int i = 0; i < 16; i++) {
		std::cout << output_naive[i] << std::endl;
	}

	NTTConfig<scalar_t> config = default_ntt_config<scalar_t>();
	ConfigExtension ntt_cfg_ext;
	config.batch_size = batch_size;
	config.ordering = Ordering::kNR;

	config.ext = &ntt_cfg_ext;
	ntt(input.get(), ntt_size, NTTDir::kForward, config, output.get());

	std::cout << "OUTPUT" << std::endl;
	for (int i = 0; i < 16; i++) {
		std::cout << output[i] << std::endl;
	}

}

// namespace {

// auto get_precomputed(size_t n) -> const NTTPrecomputed & {
//     EXPECT(is_power_of_two(n));
//     static std::mutex cache_mutex;
//     static std::vector<std::optional<NTTPrecomputed>> cache(4096);

//     std::lock_guard<std::mutex> lock(cache_mutex);
//     const size_t log_n = clog2(n);
//     if (const auto &p = cache[log_n]; p) {
//         return *p;
//     }

//     NTTPrecomputed p;
//     const auto omega = Scalar::twiddle(n);
//     p.omega_br.resize(n / 2);
//     cilk_for (size_t i = 0; i < n / 2; i++) {
//         p.omega_br[i] = omega.pow(bit_reverse(i, log_n - 1));
//     }
//     return (cache[log_n] = std::move(p)).value();
// }

// auto ntt_naive(std::span<Scalar> input) -> std::vector<Scalar> {
//     EXPECT(is_power_of_two(input.size()));
//     std::vector<Scalar> output(input.size());

//     const auto omega = Scalar::twiddle(input.size());
//     const size_t log_n = clog2(input.size());

//     for (size_t i = 0; i < input.size(); ++i) {
//         Scalar sum{0};
//         for (size_t j = 0; j < input.size(); ++j) {
//             sum += input[j] * omega.pow(j * i);
//         }
//         output[bit_reverse(i, log_n)] = sum;
//     }

//     return output;
// }

// } // namespace

// auto ntt(std::span<const Scalar> input, size_t n) -> std::vector<Scalar> {
//     EXPECT(is_power_of_two(n));
//     std::vector<Scalar> output(n);
//     ntt(input, n, output);
//     sync_current_stream();
//     return output;
// }

// void ntt(std::span<const Scalar> input, size_t n, std::span<Scalar> output) {
//     EXPECT(is_power_of_two(n) && output.size() == n);
//     std::copy(input.begin(), input.end(), output.begin());
//     std::fill(output.begin() + static_cast<ssize_t>(input.size()), output.end(),
//               Scalar::Consts::ZERO);
//     const auto &omega_br = get_precomputed(n).omega_br;
//     // size_t t = n / 2;
//     // for (size_t m = 1; m < n; m *= 2, t /= 2) {
//     //     cilk_for (size_t i = 0; i < m; i++) {
//     //         const size_t j_start = 2 * i * t;
//     //         const size_t j_end = j_start + t;
//     //         cilk_for (size_t j = j_start; j < j_end; j++) {
//     //             const auto tmp = output[j + t] * omega_br[i];
//     //             output[j + t] = output[j] - tmp;
//     //             output[j] = output[j] + tmp;
//     //         }
//     //     }
//     // }
//     size_t log_n = clog2(n);
//     for (size_t log_m = 0, log_t = log_n - 1; log_m < log_n; log_m++, log_t--) {
//         const size_t t = 1 << log_t;
// #ifdef NTT_DEFERRED_SYNC_ENABLED
//         tapir_deferred_sync
// #endif
//             cilk_gpu_for (size_t ij = 0; ij < (n / 2); ij++) {
//             const size_t i = ij >> log_t;
//             const size_t j = (i << (log_t + 1)) + (ij & (t - 1));
//             const auto tmp = output[j + t] * omega_br[i];
//             output[j + t] = output[j] - tmp;
//             output[j] = output[j] + tmp;
//         }
//     }
// }

// // NOLINTBEGIN(*-magic-numbers)
// TEST_CASE("Reflexive bit_reverse", "[ntt]") {
//     const size_t log_n = 5;
//     const size_t n = 1 << log_n;
//     for (size_t i = 0; i < n; i++) {
//         // Checks for reflexivity
//         const size_t br = bit_reverse(bit_reverse(i, log_n), log_n);
//         CHECK(br == i);
//     }
// }

// TEST_CASE("NTT", "[ntt]") {
//     constexpr size_t N = 16;

//     std::vector<Scalar> input(N);
//     std::mt19937_64 rng(42);

//     for (size_t i = 0; i < N; ++i) {
//         input[i] = Scalar::random(rng);
//     }

//     const auto output = ntt(input, N);
//     const auto output_naive = ntt_naive(input);

//     CHECK(output == output_naive);
// }
// // NOLINTEND(*-magic-numbers)