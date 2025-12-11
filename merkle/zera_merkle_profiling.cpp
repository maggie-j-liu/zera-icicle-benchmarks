#include "icicle/merkle/merkle_tree.h"
#include "icicle/hash/blake3.h"
#include "icicle/curves/params/bn254.h"
#include <span>
#include "cilk.h"
#include "blake3.hpp"
#include <ctimer.h>
#include <nvtx3/nvtx3.hpp>
using namespace bn254;

using BlakeHash = std::array<uint8_t, BLAKE3_OUT_LEN>;

std::string bytes_to_hex(const BlakeHash& hash) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : hash) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

template <typename T> auto next_half(std::span<T> span) -> std::span<T> {
    return std::span<T>{span.data() + span.size(), span.size() / 2};
}

double merklize(size_t n_rows, size_t n_cols, scalar_t* input) {
	ctimer_t t;
	ctimer_start(&t);
	int input_size = n_rows * n_cols;
	auto hashes = std::make_unique<BlakeHash[]>(2 * n_cols - 1);

    // std::vector<BlakeHash> hashes(2 * n_cols - 1);
	nvtx3::scoped_range r{"merklize"};

    // Start by hashing every column
    tapir_deferred_sync cilk_gpu_for (size_t i = 0; i < n_cols; i++) {
		size_t j = 0;
        const auto next_data = [&](uint32_t *msg_block) {
            auto *msg_block_64 = reinterpret_cast<scalar_t *>(msg_block);
            // Now if you omit this pragma, kitsune will miscompile.
            // WTF???
            _Pragma("unroll") for (size_t k = 0;
                                   k < blake3::BLOCK_SIZE / sizeof(scalar_t);
                                   k++, j++) {
                // const auto raw = static_cast<uint64_t>(
                //     secret.evals_encoded[j * size.n_cols + i]);
                msg_block_64[k] = input[i * n_rows + j];
            }
        };
        // whoops
        blake3::hash_multi(hashes[i].data(), n_rows * sizeof(scalar_t),
                           next_data);
    }

	
    std::span<BlakeHash> prev_layer{hashes.get(), n_cols};
    std::span<BlakeHash> cur_layer = next_half(prev_layer);
    while (cur_layer.size() >= 1) {
        tapir_deferred_sync cilk_gpu_for (size_t i = 0; i < cur_layer.size();
                                          i++) {
			blake3::hash_64(prev_layer[2 * i].data(), cur_layer[i].data());
        }
        prev_layer = cur_layer;
        cur_layer = next_half(prev_layer);
    }
    sync_current_stream();
	ctimer_stop(&t);

	ctimer_measure(&t);
	long ns = timespec_nsec(t.elapsed);
	return ns / 1000000.0;
}

void run_benchmark(int num_leaves) {
	std::cout << "\n=== Zera merkle tree, size=" << num_leaves << " ===" << std::endl;
	int n_rows = 128;
	int input_size = n_rows * num_leaves; 
	auto input = std::make_unique<scalar_t[]>(input_size);
	scalar_t::rand_host_many(input.get(), input_size);

	double total_time = 0;	

	merklize(n_rows, num_leaves, input.get());
	total_time = merklize(n_rows, num_leaves, input.get());

	double ms = total_time;

    std::cout << "avg time: "
              << ms
              << " ms" << std::endl;
}

int main() {
	int sizes[] = {1 << 18};

	for (int size : sizes) {
		run_benchmark(size);
	}
}