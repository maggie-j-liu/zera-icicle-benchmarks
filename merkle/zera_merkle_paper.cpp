// #define DEFERRED_SYNC_ENABLED
#include "icicle/merkle/merkle_tree.h"
#include "icicle/hash/blake3.h"
#include "icicle/curves/params/bn254.h"
#include <span>
#include "cilk.h"
#include "blake3.hpp"
#include <ctimer.h>
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

    std::vector<BlakeHash> hashes(2 * n_cols - 1);

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

	
    std::span<BlakeHash> prev_layer{hashes.data(), n_cols};
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

void run_benchmark(int* n_rows, int* n_cols, int trials) {
	std::cout << "\n=== Zera merkle tree, size=" << n_rows[0] * n_cols[0] << " ===" << std::endl;

	double total_time = 0.0;

	for (int sz_idx = 0; sz_idx < 2; sz_idx++) {
		int input_size = n_rows[sz_idx] * n_cols[sz_idx]; 
		auto input = std::make_unique<scalar_t[]>(input_size);
		scalar_t::rand_host_many(input.get(), input_size);

		for (int i = 0; i < 3; i++) {
			merklize(n_rows[sz_idx], n_cols[sz_idx], input.get());
		}

		double time = 0.0;
		for (int i = 0; i < trials; i++) {
			time += merklize(n_rows[sz_idx], n_cols[sz_idx], input.get());
		}
		total_time += time;
		std::cout << "n_rows: " << n_rows[sz_idx] << " n_cols: " << n_cols[sz_idx] << " avg time: " << time / trials << std::endl;
		
	}
	double ms = total_time / trials;

	std::cout << "avg time: "
			<< ms
			<< " ms" << std::endl;
}

int main() {
	int n_rows[] = {32, 64, 32, 128, 64, 128, 64, 256};
	int n_cols[] = {8192, 32768, 16384, 32768, 16384, 65536, 32768, 65536};
    int trials = 10;

	for (int i = 0; i < 4; i++) {
		run_benchmark(n_rows + i * 2, n_cols + i * 2, trials);
	}
}