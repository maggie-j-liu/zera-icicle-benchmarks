#define FIELD_ID BN254
#define NDEBUG

#include "icicle/program/returning_value_program.h"
#include "icicle/hash/blake3.h"
#include "icicle/sumcheck/sumcheck.h"
#include "icicle/curves/params/bn254.h"
#include <iostream>
#include <ctimer.h>

using namespace bn254;

std::pair<double, double> run_benchmark_once(int log_mle_poly_size) {
    // std::cout << "\nIcicle Examples: Sumcheck with EQ * (A * B - C) combine function" << std::endl;

    int mle_poly_size = 1 << log_mle_poly_size;
    int nof_mle_poly = 4;

    // std::cout << "\nGenerating input data" << std::endl;
    // generate inputs
    std::vector<scalar_t*> mle_polynomials(nof_mle_poly);
    for (int poly_i = 0; poly_i < nof_mle_poly; poly_i++) {
        mle_polynomials[poly_i] = new scalar_t[mle_poly_size];
        scalar_t::rand_host_many(mle_polynomials[poly_i], mle_poly_size);
    }

	std::vector<scalar_t*> device_mle_polynomials(nof_mle_poly);
    for (int poly_i = 0; poly_i < nof_mle_poly; poly_i++) {
		icicle_malloc((void **)&device_mle_polynomials[poly_i], sizeof(scalar_t) * mle_poly_size);
        icicle_copy(device_mle_polynomials[poly_i], mle_polynomials[poly_i], sizeof(scalar_t) * mle_poly_size);
    }

    // std::cout << "Calculating sum" << std::endl;
    // calculate the claimed sum
    scalar_t claimed_sum = scalar_t::zero();
    for (int element_i = 0; element_i < mle_poly_size; element_i++) {
        const scalar_t a = mle_polynomials[0][element_i];
        const scalar_t b = mle_polynomials[1][element_i];
        const scalar_t c = mle_polynomials[2][element_i];
        const scalar_t eq = mle_polynomials[3][element_i];
        claimed_sum = claimed_sum + (a * b - c) * eq;
    }

    Hash hasher = Blake3::create();
    const char* domain_label = "ingonyama";
    const char* poly_label = "poly_label";
    const char* challenge_label = "icicle";
    scalar_t seed = scalar_t::from(18);
    bool little_endian = true;

    // create sumcheck
    auto prover_sumcheck = create_sumcheck<scalar_t>();
    // create the combine function to be the pre-defined function eq*(a*b-c)
    CombineFunction<scalar_t> combine_func(EQ_X_AB_MINUS_C);
    const int nof_inputs = 4;

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

	int sizes[] = {18, 20, 22};
	int trials = 10;

	for (int size : sizes) {
    	run_benchmark(size, trials);
	}
    
}