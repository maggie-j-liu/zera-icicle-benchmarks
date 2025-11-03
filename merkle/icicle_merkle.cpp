#include "icicle/merkle/merkle_tree.h"
#include "icicle/hash/blake3.h"
#include "icicle/curves/params/bn254.h"
using namespace bn254;

int main() {
	const uint64_t leaf_n_elements = 128;
	int layers = 5;
	const uint64_t leaf_size = leaf_n_elements * sizeof(scalar_t);
	// Allocate a dummy input. It can be any type as long as the total size matches.
	const uint32_t max_input_size = leaf_size * (1 << (layers - 1));
	auto input = std::make_unique<scalar_t[]>(max_input_size / sizeof(scalar_t));
	scalar_t::rand_host_many(input.get(), max_input_size / sizeof(scalar_t));

	// Define hashers
	auto hasher = Blake3::create(leaf_size); // hash 1KB -> 32B
	auto compress = Blake3::create(2 * hasher.output_size()); // hash every 64B to 32B

	// Construct the tree using the layer hashers and leaf-size
	std::vector<Hash> hashers = {hasher, compress, compress, compress, compress};
	auto merkle_tree = MerkleTree::create(hashers, leaf_size);

	// compute the tree
	merkle_tree.build(input.get(), max_input_size / sizeof(scalar_t), default_merkle_tree_config());
}