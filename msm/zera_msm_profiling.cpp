#include <array>
#include <atomic>
#include <fstream>
#include <span>
#include <vector>
#include <ctimer.h>

#include "scan.h"

#include <cilk/cilk.h>

#include "icicle/runtime.h"
#include "icicle/msm.h"
#include "icicle/curves/params/bls12_377.h"

#include <nvtx3/nvtx3.hpp>


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

projective_t zera_msm(const scalar_t *scalars, const affine_t* points, int size) {
    // Step 0: window split.
    std::vector<uint32_t> split_scalars[N_WINDOWS];
    std::vector<std::span<uint32_t>> split_scalars_span(N_WINDOWS);

    cilk_for (size_t i = 0; i < N_WINDOWS; i++) {
        split_scalars[i].resize(size);
        split_scalars_span[i] = split_scalars[i];
    }
    {
        nvtx3::scoped_range r{"split_scalars"};
        cilk_gpu_for (size_t i = 0; i < size; i++) {
            const auto s = split<256, WINDOW_SIZE>(scalars[i]);
            for (size_t w = 0; w < N_WINDOWS; w++) 
                split_scalars_span[w][i] = s[w];
        }
    }
    std::vector<projective_t> bucket_sums(N_WINDOWS);
    cilk_for (size_t w = 0; w < N_WINDOWS; w++) {
        std::span<uint32_t> bucket_idx = split_scalars_span[w];

        std::vector<std::atomic<int>> bucket_size(1 << WINDOW_SIZE);
        {
            nvtx3::scoped_range r{"bucket_histogram"};
            cilk_gpu_for (size_t i = 0; i < size; i++) {
                // Histogram
                bucket_size[bucket_idx[i]].fetch_add(1, std::memory_order_relaxed);
            }
        }
        

        auto bucket_offset = std::make_unique_for_overwrite<int[]>(1 << WINDOW_SIZE);
        scanner<decltype(bucket_offset)> scan_bucket_offset(bucket_offset);
        {
            nvtx3::scoped_range r{"scan bucket offset"};
            cilk_gpu_for (size_t i = 0; i < (1 << WINDOW_SIZE); i++) {
                auto v = scan_bucket_offset.view(bucket_offset, i);
                *v += i ? bucket_size[i].load(std::memory_order_relaxed) : 0;
            }
        }

        const size_t total_size = bucket_offset.get()[(1 << WINDOW_SIZE) - 1];

        std::vector<std::atomic<int>> bucket_start(1 << WINDOW_SIZE);
        {
            nvtx3::scoped_range r{"bucket start"};
            cilk_gpu_for (size_t i = 1; i < (1 << WINDOW_SIZE); i++) {
                bucket_start[i].store(bucket_offset[i - 1], std::memory_order_relaxed);
            }
        }

        auto reordered = std::make_unique_for_overwrite<size_t[]>(total_size);
        {
            nvtx3::scoped_range r{"reorder"};
            cilk_gpu_for (size_t i = 0; i < size; i++) {
                if (const uint32_t key = bucket_idx[i]; key) {
                    int idx = bucket_start[key].fetch_add(1, std::memory_order_relaxed);
                    reordered[idx] = i;
                }
            }
        }
        
        auto reordered_scan = std::make_unique_for_overwrite<projective_t[]>(total_size);
        scanner<decltype(reordered_scan), id_projective, reduce_projective> scan_bucket(
            reordered_scan);
        {
            nvtx3::scoped_range r{"bucket scan accumulation"};
            cilk_gpu_for (size_t i = 0; i < total_size; i++) {
                auto v = scan_bucket.view(reordered_scan, i);
                *v = *v + points[reordered[total_size - 1 - i]];
            }
        }

        projective_t cilk_reducer(id_projective, reduce_projective)
            sum{projective_t::zero()};
        {
            nvtx3::scoped_range r{"bucket final sum"};
            cilk_gpu_for (size_t i = 0; i < (1 << WINDOW_SIZE) - 1; i++) {
                if (bucket_offset[i] < total_size) {
                    sum = sum + reordered_scan[total_size - 1 - bucket_offset[i]];
                }
            }
        }

        // std::cout << "Bucket sums[" << w << "] = " << sum.to_affine() << std::endl;
        bucket_sums[w] = sum;
    }

    auto sum = projective_t::zero();
    for (size_t i = N_WINDOWS; i-- > 0;) {
        for (size_t j = 0; j < WINDOW_SIZE; j++)
            sum = sum + sum;
        sum = sum + bucket_sums[i];
    }
    return sum;
}

void run_benchmark(int size) {
    std::cout << "\n=== Zera msm, size=" << size << " ===" << std::endl;
    auto scalars = std::make_unique<scalar_t[]>(size);
    auto points = std::make_unique<affine_t[]>(size);
    scalar_t::rand_host_many(scalars.get(), size);
    projective_t::rand_host_many(points.get(), size);

    projective_t sum;

    zera_msm(scalars.get(), points.get(), size);

    std::cout << "zera sum = " << sum.to_affine() << std::endl;
}


int main() {
    int sizes[] = {1 << 20};

    for (auto s : sizes) {
        run_benchmark(s);
    }

    return 0;
}
