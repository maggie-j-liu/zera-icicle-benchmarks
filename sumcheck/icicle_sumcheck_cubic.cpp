#define FIELD_ID BN254
#define NDEBUG

#include "icicle/program/returning_value_program.h"
#include "icicle/hash/blake3.h"
#include "icicle/sumcheck/sumcheck.h"
#include "icicle/curves/params/bn254.h"
#include <iostream>
#include <ctimer.h>

using namespace bn254;

using MlePoly = Symbol<scalar_t>;
MlePoly user_def_cubic_sumcheck_combine(
    const std::vector<MlePoly>& inputs)
{
    size_t n_par = (inputs.size() - 1) / 7;  // (roughly, adapt as needed)

    const size_t offset_coeff_par = 0;
    const size_t offset_a_par = offset_coeff_par + n_par;
    const size_t offset_b_par = offset_a_par + n_par;
    const size_t offset_c_par = offset_b_par + n_par; // single c_par
    const size_t offset_coeff_seq = offset_c_par + 1;
    const size_t offset_a_seq = offset_coeff_seq + n_par;
    const size_t offset_b_seq = offset_a_seq + n_par;
    const size_t offset_c_seq = offset_b_seq + n_par;

    MlePoly acc = scalar_t::zero();

    // ---- first sum ----
    for (size_t i = 0; i < n_par; i++) {
        acc = acc + inputs[offset_coeff_par + i]
                    * inputs[offset_a_par + i]
                    * inputs[offset_b_par + i]
                    * inputs[offset_c_par];
    }

    // ---- second sum ----
    for (size_t i = 0; i < n_par; i++) {
        acc = acc + inputs[offset_coeff_seq + i]
                    * inputs[offset_a_seq + i]
                    * inputs[offset_b_seq + i]
                    * inputs[offset_c_seq + i];
    }

    return acc;
}

std::pair<double, double> run_benchmark_once(int log_mle_poly_size) {
    // std::cout << "\nIcicle Examples: Sumcheck with EQ * (A * B - C) combine function" << std::endl;

    int mle_poly_size = 1 << log_mle_poly_size;

	size_t n_par = 1;
	int nof_mle_poly = 7 * n_par + 1;

    // std::cout << "\nGenerating input data" << std::endl;
    // generate inputs
    std::vector<std::vector<scalar_t>> mle_polynomials(nof_mle_poly, std::vector<scalar_t>(mle_poly_size));
    for (int poly_i = 0; poly_i < nof_mle_poly; poly_i++) {
        scalar_t::rand_host_many(mle_polynomials[poly_i].data(), mle_poly_size);
    }

	for (int i = 0; i < n_par; i++) {
		for (int x = 1; x < mle_poly_size; x++) {
			mle_polynomials[i][x] = mle_polynomials[i][0];
			mle_polynomials[3 * n_par + 1 + i][x] = mle_polynomials[3 * n_par + 1 + i][0];
		}
	}

	std::vector<scalar_t*> device_mle_polynomials(nof_mle_poly);
    for (int poly_i = 0; poly_i < nof_mle_poly; poly_i++) {
		icicle_malloc((void **)&device_mle_polynomials[poly_i], sizeof(scalar_t) * mle_poly_size);
        icicle_copy(device_mle_polynomials[poly_i], mle_polynomials[poly_i].data(), sizeof(scalar_t) * mle_poly_size);
    }

    // std::cout << "Calculating sum" << std::endl;
    // calculate the claimed sum
    scalar_t claimed_sum = scalar_t::zero();
	for (int i = 0; i < n_par; i++) {
		for (int x = 0; x < mle_poly_size; x++) {
			claimed_sum = claimed_sum + mle_polynomials[i][0] * mle_polynomials[n_par + i][x] * mle_polynomials[2 * n_par + i][x] * mle_polynomials[3 * n_par][x];
		}
	}
	for (int i = 0; i < n_par; i++) {
		for (int x = 0; x < mle_poly_size; x++) {
			claimed_sum = claimed_sum + mle_polynomials[3 * n_par + 1 + i][0] * mle_polynomials[4 * n_par + 1 + i][x] * mle_polynomials[5 * n_par + 1 + i][x] * mle_polynomials[6 * n_par + 1 + i][x];
		}
	}

    Hash hasher = Blake3::create();
    const char* domain_label = "ingonyama";
    const char* poly_label = "poly_label";
    const char* challenge_label = "icicle";
    scalar_t seed = scalar_t::from(18);
    bool little_endian = true;

    // create sumcheck
    auto prover_sumcheck = create_sumcheck<scalar_t>();

	CombineFunction<scalar_t> combine_func(user_def_cubic_sumcheck_combine, nof_mle_poly);

    // create default sumcheck config
    SumcheckConfig sumcheck_config;
	sumcheck_config.are_inputs_on_device = true;

    // create empty sumcheck proof object which the prover will assign round polynomials into
    SumcheckProof<scalar_t> sumcheck_proof;

	// create transcript_config for Fiat-Shamir
	SumcheckTranscriptConfig<scalar_t> prover_transcript_config(
		hasher, domain_label, poly_label, challenge_label, seed, little_endian);

	ctimer_t t;
	ctimer_start(&t);
	// create the proof - Prover side
	prover_sumcheck.get_proof(
		device_mle_polynomials, mle_poly_size, claimed_sum, combine_func, std::move(prover_transcript_config), sumcheck_config,
		sumcheck_proof);
	ctimer_stop(&t);
	ctimer_measure(&t);

	long ns = timespec_nsec(t.elapsed);
  	double prover_time = ns / 1000000.0;

    // create sumcheck object for the Verifier
    auto verifier_sumcheck = create_sumcheck<scalar_t>();
    // create boolean variable for verification output
    bool verification_pass = false;

    // std::cout << "Verifying proof" << std::endl;

	// verify the proof - Verifier side
	// NOTE: the transcript config should be identical for both Prover and Verifier
	SumcheckTranscriptConfig<scalar_t> verifier_transcript_config(
		hasher, domain_label, poly_label, challenge_label, seed, little_endian);

	
	ctimer_start(&t);
	verifier_sumcheck.verify(sumcheck_proof, claimed_sum, std::move(verifier_transcript_config), verification_pass);
	ctimer_stop(&t);
	ctimer_measure(&t);

    ns = timespec_nsec(t.elapsed);
  	double verifier_time = ns / 1000000.0;

	if (!verification_pass) {
		std::cout << "ERROR: Verification mismatch" << std::endl;
	}
    
	for (int poly_i = 0; poly_i < nof_mle_poly; poly_i++) {
        icicle_free(device_mle_polynomials[poly_i]);
    }

	return {prover_time, verifier_time};
}

void run_benchmark(int log_mle_poly_size, int trials) {
	std::cout << "\n=== ICICLE sumcheck, log size=" << log_mle_poly_size << " ===" << std::endl;
	for (int i = 0; i < 3; i++) {
		run_benchmark_once(log_mle_poly_size);
	}

	double prover_tot = 0;
	double verifier_tot = 0;
	for (int i = 0; i < trials; i++) {
		auto [prover_time, verifier_time] = run_benchmark_once(log_mle_poly_size);
		prover_tot += prover_time; 
		verifier_tot += verifier_time;
	}

	std::cout << "prover avg time: "
              << prover_tot / trials
              << " ms" << std::endl;
	std::cout << "verifier avg time: "
              << verifier_tot / trials
              << " ms" << std::endl;
}

int main(int argc, char* argv[])
{
    icicle_load_backend_from_env_or_default();

    Device device_cpu = {"CPU", 0};
    Device device_gpu = {"CUDA", 0};
    if (icicle_is_device_available("CUDA") != eIcicleError::SUCCESS) {
        std::cout << "CUDA not available, using CPU only" << std::endl;
        device_gpu = device_cpu;
    }

    icicle_set_device(device_gpu);

	int sizes[] = {16, 18, 20};
	int trials = 10;

	for (int size : sizes) {
    	run_benchmark(size, trials);
	}
}