#include "icicle/merkle/merkle_tree.h"
#include "icicle/hash/blake3.h"
#include "icicle/curves/params/bn254.h"
#include <span>
#include "cilk.h"
#include "blake3.hpp"
#include <cmath>
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

std::string bytes_to_hex(const std::byte* data, size_t n) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < n; ++i)
        ss << std::setw(2) << (static_cast<unsigned int>(data[i]) & 0xff);
    return ss.str();
}

template <typename T> auto next_half(std::span<T> span) -> std::span<T> {
    return std::span<T>{span.data() + span.size(), span.size() / 2};
}

auto merklize(size_t n_rows, size_t n_cols, scalar_t* input)
    -> std::vector<BlakeHash> {
	int input_size = n_rows * n_cols;

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
	std::cout << "done" << std::endl;
    return hashes;
}

MerkleTree icicle_merklize(size_t n_rows, size_t n_cols, scalar_t* input) {
	// Define hashers
	auto hasher = Blake3::create(n_rows * sizeof(scalar_t)); // hash 1KB -> 32B
	auto compress = Blake3::create(2 * hasher.output_size()); // hash every 64B to 32B

	// Construct the tree using the layer hashers and leaf-size
	std::vector<Hash> hashers = {hasher};

	int compress_layers = std::log2(n_cols);
	for (int i = 0; i < compress_layers; i++) {
		hashers.push_back(compress);
	}
	
	auto merkle_tree = MerkleTree::create(hashers, n_rows * sizeof(scalar_t));

	// compute the tree
	merkle_tree.build(input, n_rows * n_cols, default_merkle_tree_config());

	return merkle_tree;
}

int main() {
	const uint64_t leaf_n_elements = 128;
	int layers = 5;

	size_t n_rows = leaf_n_elements;
	size_t n_cols = 1 << (layers - 1);
	int input_size = n_rows * n_cols;
	auto input = std::make_unique<scalar_t[]>(input_size);
	// for (uint32_t i = 0; i < input_size; i++) {
	// 	input[i] = scalar_t::one(); 
	// }
	scalar_t::rand_host_many(input.get(), input_size);

	std::vector<BlakeHash> hashes = merklize(n_rows, n_cols, input.get());

	BlakeHash zera_root = hashes.back();

	std::cout << "zera root hash " << bytes_to_hex(zera_root) << std::endl;

	MerkleTree merkle_tree = icicle_merklize(n_rows, n_cols, input.get());

	auto [commitment, size] = merkle_tree.get_merkle_root();
	std::cout << "icicle root hash: " << bytes_to_hex(commitment, size) << "\n";

	bool match = true;
	for (int i = 0; i < BLAKE3_OUT_LEN; i++) {
		if (zera_root[i] != static_cast<uint8_t>(commitment[i])) {
			match = false;
			break;
		}
	}

	std::cout << "match " << match << std::endl; 
}