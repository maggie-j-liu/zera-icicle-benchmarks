#include <array>
#include <atomic>
#include <fstream>
#include <span>
#include <vector>

#include "scan.h"

#include <cilk/cilk.h>

#include "icicle/runtime.h"
#include "icicle/msm.h"
#include "icicle/curves/params/bls12_377.h"
using namespace bls12_377;

template <size_t N, size_t W>
auto split(const scalar_t &scalar) -> std::array<uint32_t, (N + W - 1) / W> {
    static_assert(W <= 32, "Window size must fit into 32 bits");
    constexpr size_t N_WINDOWS = (N + W - 1) / W;

    std::array<uint32_t, N_WINDOWS> result{};

    for (size_t i = 0; i < N_WINDOWS; i++) {
        const unsigned bit_start = i * W;
        const unsigned width = std::min<unsigned>(W, N - bit_start);
        result[i] = scalar.get_scalar_digit(i, width);
    }

    return result;
}

#define cilk_gpu_for [[tapir::target("cuda"), tapir::grain_size(1)]] cilk_for
#define scanner kitcuda::scanner

namespace {

constexpr size_t WINDOW_SIZE = 16;
constexpr size_t N_WINDOWS = (253 + WINDOW_SIZE - 1) / WINDOW_SIZE;

void id_projective(void *v) {
    // NOLINTNEXTLINE(*-reinterpret-cast)
    *reinterpret_cast<projective_t *>(v) = projective_t::zero();
}

void reduce_projective(void *left, void *right) {
    // NOLINTNEXTLINE(*-reinterpret-cast)
    auto *lhs = reinterpret_cast<projective_t *>(left);
    auto *rhs = reinterpret_cast<projective_t *>(right);
    *lhs = *lhs + *rhs;
}

} // namespace

int main() {
    using namespace std::chrono;

    auto start = high_resolution_clock::now();

    int N = 1 << 10;
    
    auto scalars = std::make_unique<scalar_t[]>(N);
    auto points = std::make_unique<affine_t[]>(N);
    projective_t result;
    scalar_t::rand_host_many(scalars.get(), N);
    projective_t::rand_host_many(points.get(), N);

    auto end = high_resolution_clock::now();
    std::cout << "Reading data took "
              << duration_cast<milliseconds>(end - start).count()
              << " ms" << std::endl;

    // Step 0: window split.
    start = high_resolution_clock::now();

    std::vector<uint32_t> split_scalars[N_WINDOWS];
    std::vector<std::span<uint32_t>> split_scalars_span(N_WINDOWS);

    cilk_for (size_t i = 0; i < N_WINDOWS; i++) {
        split_scalars[i].resize(N);
        split_scalars_span[i] = split_scalars[i];
    }
    cilk_gpu_for (size_t i = 0; i < N; i++) {
        const auto s = split<256, WINDOW_SIZE>(scalars[i]);
        for (size_t w = 0; w < N_WINDOWS; w++) 
            split_scalars_span[w][i] = s[w];
    }
    std::vector<projective_t> bucket_sums(N_WINDOWS);
    cilk_for (size_t w = 0; w < N_WINDOWS; w++) {
        std::span<uint32_t> bucket_idx = split_scalars_span[w];

        std::vector<std::atomic<int>> bucket_size(1 << WINDOW_SIZE);
        cilk_gpu_for (size_t i = 0; i < N; i++) {
            // Histogram
            bucket_size[bucket_idx[i]].fetch_add(1, std::memory_order_relaxed);
        }

        auto bucket_offset = std::make_unique_for_overwrite<int[]>(1 << WINDOW_SIZE);
        scanner<decltype(bucket_offset)> scan_bucket_offset(bucket_offset);
        cilk_gpu_for (size_t i = 0; i < (1 << WINDOW_SIZE); i++) {
            auto v = scan_bucket_offset.view(bucket_offset, i);
            *v += i ? bucket_size[i].load(std::memory_order_relaxed) : 0;
        }

        const size_t total_size = bucket_offset.get()[(1 << WINDOW_SIZE) - 1];

        std::vector<std::atomic<int>> bucket_start(1 << WINDOW_SIZE);
        cilk_gpu_for (size_t i = 1; i < (1 << WINDOW_SIZE); i++) {
            bucket_start[i].store(bucket_offset[i - 1], std::memory_order_relaxed);
        }

        auto reordered = std::make_unique_for_overwrite<size_t[]>(total_size);
        cilk_gpu_for (size_t i = 0; i < N; i++) {
            if (const uint32_t key = bucket_idx[i]; key) {
                int idx = bucket_start[key].fetch_add(1, std::memory_order_relaxed);
                reordered[idx] = i;
            }
        }

        auto reordered_scan = std::make_unique_for_overwrite<projective_t[]>(total_size);
        scanner<decltype(reordered_scan), id_projective, reduce_projective> scan_bucket(
            reordered_scan);
        cilk_gpu_for (size_t i = 0; i < total_size; i++) {
            auto v = scan_bucket.view(reordered_scan, i);
            *v = *v + points[reordered[total_size - 1 - i]];
        }

        // projective_t cilk_reducer(id_projective, reduce_projective)
        //     sum{projective_t::zero()};
        // cilk_gpu_for (size_t i = 0; i < (1 << WINDOW_SIZE) - 1; i++) {
        //     if (bucket_offset[i] < total_size) {
        //         sum = sum + reordered_scan[total_size - 1 - bucket_offset[i]];
        //     }
        // }

        // std::cout << "Bucket sums[" << w << "] = " << sum.to_affine() << std::endl;
        // bucket_sums[w] = sum;
    }

    // auto sum = projective_t::zero();
    // for (size_t i = N_WINDOWS; i-- > 0;) {
    //     for (size_t j = 0; j < WINDOW_SIZE; j++)
    //         sum = sum + sum;
    //     sum = sum + bucket_sums[i];
    // }

    // end = high_resolution_clock::now();
    // std::cout << "Total time is "
    //           << duration_cast<milliseconds>(end - start).count()
    //           << " ms" << std::endl;
    // std::cout << "sum = " << sum.to_affine() << std::endl;

}
