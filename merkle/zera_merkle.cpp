#include "icicle/merkle/merkle_tree.h"
#include "icicle/hash/blake3.h"
#include "icicle/curves/params/bn254.h"
#include <span>
#include "cilk.h"
using namespace bn254;

constexpr size_t BLAKE3_OUT_LEN = 32;
using BlakeHash = std::array<std::byte, BLAKE3_OUT_LEN>;

template <typename T> auto next_half(std::span<T> span) -> std::span<T> {
    return std::span<T>{span.data() + span.size(), span.size() / 2};
}

auto merklize(size_t n_rows, size_t n_cols, scalar_t* input)
    -> std::vector<BlakeHash> {
	int input_size = n_rows * n_cols;

    std::vector<BlakeHash> hashes(2 * n_cols - 1);

    // Start by hashing every column
    tapir_deferred_sync cilk_gpu_for (size_t i = 0; i < n_cols; i++) {
		auto hasher = Blake3::create();
		auto config = default_hash_config();

		// std::vector<scalar_t> values(n_rows);

        // for (size_t j = 0; j < n_rows; j++) {
		// 	values[j] = input[j * n_cols + i];
        // }

		// hasher.hash<scalar_t, std::byte>(values.data(), values.size(), config, hashes[i].data());
    }

    // std::span<BlakeHash> prev_layer{hashes.data(), n_cols};
    // std::span<BlakeHash> cur_layer = next_half(prev_layer);
    // while (cur_layer.size() >= 1) {
    //     tapir_deferred_sync cilk_gpu_for (size_t i = 0; i < cur_layer.size();
    //                                       i++) {
	// 		auto hasher = Blake3::create();
	// 		auto config = default_hash_config();	

	// 		hasher.hash(prev_layer[2 * i].data(), BLAKE3_OUT_LEN * 2, config, cur_layer[i].data());
    //     }
    //     prev_layer = cur_layer;
    //     cur_layer = next_half(prev_layer);
    // }
    sync_current_stream();
    return hashes;
}

int main() {
	const uint64_t leaf_n_elements = 128;
	int layers = 5;

	size_t n_rows = leaf_n_elements;
	size_t n_cols = 1 << (layers - 1);
	int input_size = n_rows * n_cols;
	auto input = std::make_unique<scalar_t[]>(input_size);
	scalar_t::rand_host_many(input.get(), input_size);

	merklize(n_rows, n_cols, input.get());
}