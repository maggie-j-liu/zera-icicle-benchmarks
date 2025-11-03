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

/**
 * Prove, via sumcheck, the following claim:
 *   claim == (sum_{i=0}^{n_par-1} coeff[i] *
 *             sum_{x in {0,1}^n} a_par[i](x) * b_bar[i](x) * c_par(x))
 *          + (sum_{i=0}^{n_seq-1} coeff[n_par + i] *
 *             sum_{x in {0,1}^n} a_seq[i](x) * b_seq[i](x) * c_seq[i](x))
 * where a_par, b_par, c_par, a_seq, b_seq, c_seq are all n-variate multilinear
 * dense polynomials.
 */
auto prove_sumcheck(
    SumcheckTranscript<scalar_t> &pt, scalar_t claim,
    scalar_t** a_par, scalar_t** b_par,
    scalar_t* c_par, scalar_t** a_seq,
    scalar_t** b_seq, scalar_t** c_seq,
    scalar_t* coeffs, size_t log_n_vars, size_t n_par, size_t n_seq,
	SumcheckProof<scalar_t>& proof) -> std::vector<scalar_t> {
	size_t n_vars = 1 << log_n_vars;
	proof.init(log_n_vars, 3);
    // The random vector the verifier has given us so far
    std::vector<scalar_t> rs;
	size_t remaining_poly_size = n_vars;
    for (size_t round = 0; round < log_n_vars; round++) {
        // Compute the univariate polynomial
        //   p(x_round) = sum_{x in {0,1}^{n_vars-round-1}} f(rs, x_round, x)
        // where f(x) = sum_{i=0}^{n_par-1} coeff[i]
        //                 * a_par[i](x) * b_bar[i](x) * c_par(x)
        //            + sum_{i=0}^{n_seq-1} coeff[n_par + i]
        //                 * a_seq[i](x) * b_seq[i](x) * c_seq[i](x)

        ScalarAddReducer p0{0}, p2{0}, p3{0};
        cilk_for (size_t i = 0; i < n_par + n_seq; i++) {
            // Obtain dense representations of the polynomials
            const scalar_t* a =
                (i < n_par ? a_par[i] : a_seq[i - n_par]);
            const scalar_t* b =
                (i < n_par ? b_par[i] : b_seq[i - n_par]);
            const scalar_t* c =
                (i < n_par ? c_par : c_seq[i - n_par]);

            const size_t half = remaining_poly_size / 2;

            // Now compute
            //   sum_{x in {0,1}^{n_vars-round-1}}
            //       a(rs, x_round, x) * b(rs, x_round, x) * c(rs, x_round, x)
            // Let's zoom in on a as an example. Induction hypothesis: the array
            // a here stores the dense representation of a(rs, x_, x) as a
            // (n_vars-round)-degree polynomial. In other words, a is a
            // 2^(n_vars-round)-long array storing a(rs, x_, x) for x_||x in
            // {0,1}^{n_vars-round}. By MLE construction,
            //     a(rs, x_, x) = (1-x_)*a(rs, 0, x) + x_*a(rs, 1, x).
            // where a(rs, 0, x) is the first half of the vector, while a(rs, 1,
            // x) is the second half.
            ScalarAddReducer lp0{0}, lp2{0}, lp3{0};
            cilk_gpu_for (size_t j = 0; j < half; j++) {
                lp0 = lp0 + a[j] * b[j] * c[j]; // x_round = 0
                const scalar_t a2 = a[j + half] + a[j + half] - a[j],
                             b2 = b[j + half] + b[j + half] - b[j],
                             c2 = c[j + half] + c[j + half] - c[j];
                lp2 = lp2 + a2 * b2 * c2; // x_round = 2
                const scalar_t a3 = a2 + a[j + half] - a[j],
                             b3 = b2 + b[j + half] - b[j],
                             c3 = c2 + c[j + half] - c[j];
                lp3 = lp3 + a3 * b3 * c3; // x_round = 3
            }

            p0 = p0 + coeffs[i] * lp0;
            p2 = p2 + coeffs[i] * lp2;
            p3 = p3 + coeffs[i] * lp3;
        }
        const scalar_t p1 = claim - p0; // This follows by algebra

		std::vector<scalar_t>& round_poly = proof.get_round_polynomial(round);
		round_poly[0] = p0;
		round_poly[1] = p1;
		round_poly[2] = p2;
		round_poly[3] = p3;
        const scalar_t r = pt.get_alpha(round_poly);
        rs.emplace_back(r);

        const auto r_ = std::make_unique<const scalar_t>(r);
        cilk_scope {
            cilk_spawn { bound_top_var(c_par, remaining_poly_size, *r_); }
            cilk_spawn {
                cilk_for (size_t i = 0; i < n_par; i++) {
                    cilk_scope {
                        cilk_spawn { bound_top_var(a_par[i], remaining_poly_size, *r_); }
                        bound_top_var(b_par[i], remaining_poly_size, *r_);
                    }
                }
            }
            cilk_for (size_t i = 0; i < n_seq; i++) {
                cilk_scope {
                    cilk_spawn { bound_top_var(a_seq[i], remaining_poly_size, *r_); }
                    cilk_spawn { bound_top_var(b_seq[i], remaining_poly_size, *r_); }
                    bound_top_var(c_seq[i], remaining_poly_size, *r_);
                }
            }
        }
		remaining_poly_size /= 2;
        sync_current_stream();
        claim = cubic_interpolate(p0, p1, p2, p3, r);
    }
    // for (size_t i = 0; i < n_par; i++) {
    //     prefetch_to_host(a_par[i]->evals()[0]);
    //     prefetch_to_host(b_par[i]->evals()[0]);
    // }
    // prefetch_to_host(c_par.evals()[0]);
    // for (size_t i = 0; i < n_seq; i++) {
    //     prefetch_to_host(a_seq[i]->evals()[0]);
    //     prefetch_to_host(b_seq[i]->evals()[0]);
    //     prefetch_to_host(c_seq[i]->evals()[0]);
    // }
    return rs;
}

double run_benchmark_once(int log_mle_poly_size) {
	int mle_poly_size = 1 << log_mle_poly_size;
    int nof_mle_poly = 4;

    // std::cout << "\nGenerating input data" << std::endl;
    // generate inputs
	size_t n_par = 20;
	std::vector<std::vector<scalar_t>> a_par(n_par, std::vector<scalar_t>(mle_poly_size));
    std::vector<std::vector<scalar_t>> b_par(n_par, std::vector<scalar_t>(mle_poly_size));
    std::vector<std::vector<scalar_t>> a_seq(n_par, std::vector<scalar_t>(mle_poly_size));
    std::vector<std::vector<scalar_t>> b_seq(n_par, std::vector<scalar_t>(mle_poly_size));
    std::vector<std::vector<scalar_t>> c_seq(n_par, std::vector<scalar_t>(mle_poly_size));
	for (size_t i = 0; i < n_par; i++) {
        scalar_t::rand_host_many(a_par[i].data(), mle_poly_size);
        scalar_t::rand_host_many(b_par[i].data(), mle_poly_size);
        scalar_t::rand_host_many(a_seq[i].data(), mle_poly_size);
        scalar_t::rand_host_many(b_seq[i].data(), mle_poly_size);
        scalar_t::rand_host_many(c_seq[i].data(), mle_poly_size);
    }

	std::vector<scalar_t> c_par(mle_poly_size);
    scalar_t::rand_host_many(c_par.data(), mle_poly_size);

	std::vector<scalar_t> coeffs(2 * n_par);
    scalar_t::rand_host_many(coeffs.data(), 2 * n_par);

	scalar_t claimed_sum = scalar_t::zero();
	for (int i = 0; i < n_par; i++) {
		for (int x = 0; x < mle_poly_size; x++) {
			claimed_sum = claimed_sum + a_par[i][x] * b_par[i][x] * c_par[x] * coeffs[i];
		}
	}
	for (int i = 0; i < n_par; i++) {
		for (int x = 0; x < mle_poly_size; x++) {
			claimed_sum = claimed_sum + a_seq[i][x] * b_seq[i][x] * c_seq[i][x] * coeffs[i + n_par];
		}
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

	// create transcript_config for Fiat-Shamir
	SumcheckTranscriptConfig<scalar_t> prover_transcript_config(
		hasher, domain_label, poly_label, challenge_label, seed, little_endian);
	
	const int nof_rounds = log_mle_poly_size;
	int combine_func_poly_degree = 3;
	
	SumcheckTranscript<scalar_t> sumcheck_transcript(
        claimed_sum, nof_rounds, combine_func_poly_degree, std::move(prover_transcript_config));
	
	SumcheckProof<scalar_t> sumcheck_proof;

	std::vector<scalar_t*> a_par_ptrs(n_par), b_par_ptrs(n_par);
    std::vector<scalar_t*> a_seq_ptrs(n_par), b_seq_ptrs(n_par), c_seq_ptrs(n_par);
    for (size_t i = 0; i < n_par; i++) {
        a_par_ptrs[i] = a_par[i].data();
        b_par_ptrs[i] = b_par[i].data();
        a_seq_ptrs[i] = a_seq[i].data();
        b_seq_ptrs[i] = b_seq[i].data();
        c_seq_ptrs[i] = c_seq[i].data();
    }

	ctimer_t t;
	ctimer_start(&t);
	prove_sumcheck(sumcheck_transcript, claimed_sum, a_par_ptrs.data(), b_par_ptrs.data(), c_par.data(), a_seq_ptrs.data(), b_seq_ptrs.data(), c_seq_ptrs.data(), coeffs.data(), log_mle_poly_size, n_par, n_par, sumcheck_proof);
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

	int sizes[] = {16, 18, 20};
	int trials = 10;

	for (int size : sizes) {
    	run_benchmark(size, trials);
	}
}