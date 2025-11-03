#pragma once

/**
 * Number theoretic transform (NTT) and inverse NTT (INTT) implementations.
 */

inline static DEVICE_HOST auto bit_reverse(size_t x, size_t log_n) -> size_t {
    size_t res = 0;
    for (size_t i = 0; i < log_n; i++) {
        res = (res << 1) | (x & 1);
        x >>= 1;
    }
    return res;
}

/**
 * Compute the NTT of the input 0-padded to length n.
 *
 * Given input x, the NTT sequence X is computed as
 *   X[i] = sum_{j = 0}^{n - 1} omega_n^(i*j) * x[j]
 * where omega_n is the nth root of unity.
 *
 * Basically, think of x as the coefficients of a n-degree polynomial, and NTT
 * evaluates the polynomial at the nth roots of unity.
 */
auto ntt(std::span<const Scalar> input, size_t n) -> std::vector<Scalar>;

/**
 * Compute the NTT of the input to a pre-allocated output.
 */
void ntt(std::span<const Scalar> input, size_t n, std::span<Scalar> output);