#define FIELD_ID BN254
#define NDEBUG
#include "icicle/program/returning_value_program.h"
#include "icicle/hash/blake3.h"
#include "icicle/sumcheck/sumcheck.h"
#include "icicle/curves/params/bn254.h"
#include <iostream>
#include "icicle/sumcheck/sumcheck_transcript.h"
#include <ctimer.h>

#ifdef DEFERRED_SYNC_ENABLED
#define sync_current_stream() __kitcuda_sync_current_stream()
#define tapir_deferred_sync [[tapir::deferred_sync]]
#else
#define sync_current_stream()
#define tapir_deferred_sync
#endif

#define cilk_gpu_for [[tapir::target("cuda"), tapir::grain_size(1)]] cilk_for

using namespace bn254;

static inline void zero_scalar(void *view) {
    *reinterpret_cast<scalar_t *>(view) = scalar_t::zero();
}

static inline void add_scalar(void *left, void *right) {
    auto *lhs = reinterpret_cast<scalar_t *>(left);
	auto *rhs = reinterpret_cast<scalar_t *>(right);
	*lhs = *lhs + *rhs;
}

using ScalarAddReducer = scalar_t cilk_reducer(zero_scalar, add_scalar);

auto cubic_interpolate(const scalar_t &p0, const scalar_t &p1, const scalar_t &p2,
                       const scalar_t &p3, const scalar_t &x) -> scalar_t {
    // Let p(x) = ax^3 + bx^2 + cx + d.
    // Simple linear equation solving here:
    //   p[x_] := a x^3 + b x^2 + c x + d;
    //   Solve[{p[0] == p0, p[1] == p1, p[2] == p2, p[3] == p3}, {a, b, c, d}]
    scalar_t half = scalar_t{2}.inverse();
    scalar_t sixth = scalar_t{6}.inverse();
    const scalar_t d = p0;
    const scalar_t a = sixth * (p1 + p1 + p1 - p2 - p2 - p2 + p3 - p0);
    const scalar_t b =
        half * (p0 + p0 - p1 - p1 - p1 - p1 - p1 + p2 + p2 + p2 + p2 - p3);
    const scalar_t c = p1 - d - a - b;
    return d + x * (c + x * (b + x * a));
}

void bound_top_var(scalar_t* evals, int size, const scalar_t &r) {
    const size_t half = size / 2;
    tapir_deferred_sync cilk_gpu_for (size_t i = 0; i < half; i++) {
        evals[i] = evals[i] + r * (evals[i + half] - evals[i]);
    }
}

auto prove_sumcheck(SumcheckTranscript<scalar_t> &pt, scalar_t* a,
                              scalar_t *b, scalar_t *c, scalar_t* d, int log_mle_poly_size, scalar_t claim, SumcheckProof<scalar_t>& proof)
    -> std::vector<scalar_t> {
	proof.init(log_mle_poly_size, 3);
	int mle_poly_size = 1 << log_mle_poly_size;

    std::vector<scalar_t> rs;
    size_t half = 1 << (log_mle_poly_size - 1);
	int remaining_log_poly_size = log_mle_poly_size;
    for (size_t round = 0; round < log_mle_poly_size; round++, half /= 2) {
		std::vector<scalar_t>& round_poly = proof.get_round_polynomial(round);
        ScalarAddReducer p0{scalar_t::zero()}, p2{scalar_t::zero()}, p3{scalar_t::zero()};
        cilk_gpu_for (size_t j = 0; j < half; j++) {
            p0 = p0 + d[j] * (a[j] * b[j] - c[j]); // x_round = 0
            const scalar_t a2 = a[j + half] + a[j + half] - a[j],
                         b2 = b[j + half] + b[j + half] - b[j],
                         c2 = c[j + half] + c[j + half] - c[j],
                         d2 = d[j + half] + d[j + half] - d[j];
            p2 = p2 + d2 * (a2 * b2 - c2); // x_round = 2
            const scalar_t a3 = a2 + a[j + half] - a[j],
                         b3 = b2 + b[j + half] - b[j],
                         c3 = c2 + c[j + half] - c[j],
                         d3 = d2 + d[j + half] - d[j];
            p3 = p3 + d3 * (a3 * b3 - c3); // x_round = 3
        }
        const scalar_t p1 = claim - p0; // This follows by algebra
		round_poly[0] = p0;
		round_poly[1] = p1;
		round_poly[2] = p2;
		round_poly[3] = p3;
		scalar_t r = pt.get_alpha(round_poly);
        rs.emplace_back(r);
        const auto r_ = std::make_unique<const scalar_t>(r);
		int size = 1 << remaining_log_poly_size;

        cilk_scope {
            cilk_spawn { bound_top_var(d, size, *r_); }
            cilk_spawn { bound_top_var(a, size, *r_); }
            cilk_spawn { bound_top_var(b, size, *r_); }
            bound_top_var(c, size, *r_);
        }

		remaining_log_poly_size--;
        sync_current_stream();
        claim = cubic_interpolate(p0, p1, p2, p3, r);
    }
    return rs;
}

double run_benchmark_once(int log_mle_poly_size) {
	int mle_poly_size = 1 << log_mle_poly_size;
    int nof_mle_poly = 4;

    // std::cout << "\nGenerating input data" << std::endl;
    // generate inputs
    std::vector<scalar_t*> mle_polynomials(nof_mle_poly);
    for (int poly_i = 0; poly_i < nof_mle_poly; poly_i++) {
        mle_polynomials[poly_i] = new scalar_t[mle_poly_size];
        scalar_t::rand_host_many(mle_polynomials[poly_i], mle_poly_size);
    }

	scalar_t claimed_sum = scalar_t::zero();
    for (int element_i = 0; element_i < mle_poly_size; element_i++) {
        const scalar_t a = mle_polynomials[0][element_i];
        const scalar_t b = mle_polynomials[1][element_i];
        const scalar_t c = mle_polynomials[2][element_i];
        const scalar_t eq = mle_polynomials[3][element_i];
        claimed_sum = claimed_sum + (a * b - c) * eq;
    }

	// std::cout << "claimed sum " << claimed_sum << std::endl;

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

	// create transcript_config for Fiat-Shamir
	SumcheckTranscriptConfig<scalar_t> prover_transcript_config(
		hasher, domain_label, poly_label, challenge_label, seed, little_endian);
	
	const int nof_rounds = log_mle_poly_size;
	int combine_func_poly_degree = combine_func.get_polynomial_degree();
	
	SumcheckTranscript<scalar_t> sumcheck_transcript(
        claimed_sum, nof_rounds, combine_func_poly_degree, std::move(prover_transcript_config));
	
	SumcheckProof<scalar_t> sumcheck_proof;

	ctimer_t t;
	ctimer_start(&t);
	prove_sumcheck(sumcheck_transcript, mle_polynomials[0], mle_polynomials[1], mle_polynomials[2], mle_polynomials[3], log_mle_poly_size, claimed_sum, sumcheck_proof);
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

	
	verifier_sumcheck.verify(sumcheck_proof, claimed_sum, std::move(verifier_transcript_config), verification_pass);

	if (!verification_pass) {
		std::cout << "ERROR: Verification mismatch" << std::endl;
	}

	return prover_time;
}

void run_benchmark(int log_mle_poly_size, int trials) {
	std::cout << "\n=== Zera sumcheck, log size=" << log_mle_poly_size << " ===" << std::endl;
	for (int i = 0; i < 3; i++) {
		run_benchmark_once(log_mle_poly_size);
	}

	double total_time = 0;
	for (int i = 0; i < trials; i++) {
		total_time += run_benchmark_once(log_mle_poly_size);
	}
	std::cout << "avg time: "
              << total_time / trials
              << " ms" << std::endl;
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

	int sizes[] = {18, 20, 22};
	int trials = 10;

	for (int size : sizes) {
    	run_benchmark(size, trials);
	}
}