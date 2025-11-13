#include "icicle/merkle/merkle_tree.h"
#include "icicle/hash/blake3.h"
#include "icicle/curves/params/bn254.h"
#include <sstream>
#include <ctimer.h>
using namespace bn254;

// helper to convert bytes → hex string
std::string bytes_to_hex(const std::byte* data, size_t n) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < n; ++i)
        ss << std::setw(2) << (static_cast<unsigned int>(data[i]) & 0xff);
    return ss.str();
}


void run_benchmark(int num_leaves, int trials) {
	std::cout << "\n=== ICICLE merkle tree, size=" << num_leaves << " ===" << std::endl;
	int n_rows = 128;
	int input_size = n_rows * num_leaves; 
	auto input = std::make_unique<scalar_t[]>(input_size);
	scalar_t::rand_host_many(input.get(), input_size);
	scalar_t* input_d;
	icicle_malloc((void**)&input_d, sizeof(scalar_t) * input_size);
	icicle_copy(input_d, input.get(), sizeof(scalar_t) * input_size);

	// Define hashers
	auto hasher = Blake3::create(n_rows * sizeof(scalar_t));
	auto compress = Blake3::create(2 * hasher.output_size());

	// Construct the tree using the layer hashers and leaf-size
	std::vector<Hash> hashers = {hasher};

	int compress_layers = std::log2(num_leaves);
	for (int i = 0; i < compress_layers; i++) {
		hashers.push_back(compress);
	}

	auto config = default_merkle_tree_config();
	config.is_leaves_on_device = true;

	for (int i = 0; i < 3; i++) {
		auto merkle_tree = MerkleTree::create(hashers, n_rows * sizeof(scalar_t));
		merkle_tree.build(input_d, n_rows * num_leaves, config);
	}

	ctimer_t t;
	ctimer_start(&t);
	for (int i = 0; i < trials; i++) {
		auto merkle_tree = MerkleTree::create(hashers, n_rows * sizeof(scalar_t));
		merkle_tree.build(input_d, n_rows * num_leaves, config);
	}
	ctimer_stop(&t);
	ctimer_measure(&t);

	long ns = timespec_nsec(t.elapsed);
	double ms = ns / 1000000.0 / trials;

    std::cout << "avg time: "
              << ms
              << " ms" << std::endl;
	
	icicle_free(input_d);
}

int main() {
	icicle_load_backend_from_env_or_default();

	Device device_cpu = {"CPU", 0};
	Device device_gpu = {"CUDA", 0};
	if (icicle_is_device_available("CUDA") != eIcicleError::SUCCESS) {
		std::cout << "CUDA not available, using CPU only" << std::endl;
		device_gpu = device_cpu;
	}

	icicle_set_device(device_gpu);
	int sizes[] = {1 << 17, 1 << 18, 1 << 19, 1 << 20};
    int trials = 10;

	for (int size : sizes) {
		run_benchmark(size, trials);
	}
}