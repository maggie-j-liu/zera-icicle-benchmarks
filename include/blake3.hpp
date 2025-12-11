// Very short BLAKE3 implementation

#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

namespace {

namespace blake3 {

using std::size_t;

#define ALWAYS_INLINE __attribute__((always_inline))

// NOLINTBEGIN(*-avoid-c-arrays, *-reinterpret-cast, *-avoid-magic-numbers,
// *-bounds-array-to-pointer-decay)

// Move these constants outside the anonymous namespace and mark them as device
// constants
constexpr uint32_t IV[8]{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                         0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

constexpr uint8_t CHUNK_START = 1 << 0;
constexpr uint8_t CHUNK_END = 1 << 1;
constexpr uint8_t PARENT = 1 << 2;
constexpr uint8_t ROOT = 1 << 3;

constexpr size_t BLOCK_SIZE = 64;
constexpr size_t CHUNK_SIZE = 1024;

ALWAYS_INLINE void g(uint32_t state[16], size_t a, size_t b, size_t c, size_t d,
                     uint32_t x, uint32_t y) {
    state[a] = state[a] + state[b] + x;
    state[d] = std::rotr(state[d] ^ state[a], 16);
    state[c] = state[c] + state[d];
    state[b] = std::rotr(state[b] ^ state[c], 12);
    state[a] = state[a] + state[b] + y;
    state[d] = std::rotr(state[d] ^ state[a], 8);
    state[c] = state[c] + state[d];
    state[b] = std::rotr(state[b] ^ state[c], 7);
};

template <size_t ROUND>
void round(uint32_t state[16],
           const uint32_t msg[BLOCK_SIZE / sizeof(uint32_t)]) {
    constexpr size_t MSG_SCHEDULE[7][16]{
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
        {2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8},
        {3, 4, 10, 12, 13, 2, 7, 14, 6, 5, 9, 0, 11, 15, 8, 1},
        {10, 7, 12, 9, 14, 3, 13, 15, 4, 0, 11, 2, 5, 8, 1, 6},
        {12, 13, 9, 11, 15, 10, 14, 8, 7, 2, 5, 3, 0, 1, 6, 4},
        {9, 14, 11, 5, 8, 12, 15, 1, 13, 3, 0, 10, 2, 6, 4, 7},
        {11, 15, 5, 0, 1, 9, 8, 6, 14, 10, 2, 12, 3, 4, 7, 13},
    };
#define M(x) msg[MSG_SCHEDULE[ROUND][x]]
    // Mix the columns.
    g(state, 0, 4, 8, 12, M(0), M(1));
    g(state, 1, 5, 9, 13, M(2), M(3));
    g(state, 2, 6, 10, 14, M(4), M(5));
    g(state, 3, 7, 11, 15, M(6), M(7));

    // Mix the diagonals.
    g(state, 0, 5, 10, 15, M(8), M(9));
    g(state, 1, 6, 11, 12, M(10), M(11));
    g(state, 2, 7, 8, 13, M(12), M(13));
    g(state, 3, 4, 9, 14, M(14), M(15));
#undef M
}

// Re-implementation of Blake3
ALWAYS_INLINE void
compress_pre(uint32_t state[16], const uint32_t cv[8],
             const uint32_t msg[BLOCK_SIZE / sizeof(uint32_t)],
             uint64_t counter, uint8_t block_len, uint8_t flags) {
    const uint32_t t0 = counter & 0xffffffff;
    const uint32_t t1 = counter >> 32;
    // clang-format off
    state[ 0] = cv[0]; state[ 1] = cv[1]; state[ 2] = cv[2]; state[ 3] = cv[3];
    state[ 4] = cv[4]; state[ 5] = cv[5]; state[ 6] = cv[6]; state[ 7] = cv[7];
    state[ 8] = IV[0]; state[ 9] = IV[1]; state[10] = IV[2]; state[11] = IV[3];
    state[12] = t0;    state[13] = t1;    state[14] = block_len; state[15] = flags;
    // clang-format on
    round<0>(state, msg);
    round<1>(state, msg);
    round<2>(state, msg);
    round<3>(state, msg);
    round<4>(state, msg);
    round<5>(state, msg);
    round<6>(state, msg);
}

ALWAYS_INLINE void
compress_in_place(uint32_t cv[8],
                  const uint32_t msg[BLOCK_SIZE / sizeof(uint32_t)],
                  uint64_t counter, uint8_t block_len, uint8_t flags) {
    uint32_t state[16];
    compress_pre(state, cv, msg, counter, block_len, flags);
    cv[0] = state[0] ^ state[8];
    cv[1] = state[1] ^ state[9];
    cv[2] = state[2] ^ state[10];
    cv[3] = state[3] ^ state[11];
    cv[4] = state[4] ^ state[12];
    cv[5] = state[5] ^ state[13];
    cv[6] = state[6] ^ state[14];
    cv[7] = state[7] ^ state[15];
}

void hash_64(const uint8_t *msg, uint8_t *out) {
    uint32_t cv[8];
    for (size_t i = 0; i < 8; i++) {
        cv[i] = IV[i];
    }
    compress_in_place(cv, reinterpret_cast<const uint32_t *>(msg), 0,
                      BLOCK_SIZE, CHUNK_START | CHUNK_END | ROOT);
    for (size_t i = 0; i < 8; i++) {
        reinterpret_cast<uint32_t *>(out)[i] = cv[i];
    }
}

template <typename NextData>
void hash_multi(uint8_t *out, size_t m, NextData &&next_data) {
    // 5-level tall stack would support 32 chunks, or 4K Scalars. That's plenty.
    constexpr size_t MAX_STACK_HEIGHT = 5;

    uint32_t msg_block[BLOCK_SIZE / sizeof(uint32_t)];
    uint32_t cv[8];
    const auto reset_cv = [&]() {
        for (size_t j = 0; j < 8; j++) {
            cv[j] = IV[j];
        }
    };
    reset_cv();

    uint32_t cv_stack[MAX_STACK_HEIGHT][8];
    size_t next_stack = 0, counter = 0;

    for (size_t chunk_offset = 0; chunk_offset < m;
         chunk_offset += CHUNK_SIZE) {
        const size_t chunk_size = std::min(CHUNK_SIZE, m - chunk_offset);
        for (size_t block_offset = 0; block_offset < chunk_size;
             block_offset += BLOCK_SIZE) {
            next_data(msg_block);
            uint8_t flag = 0;
            if (block_offset == 0) {
                flag |= CHUNK_START;
            }
            if (block_offset + BLOCK_SIZE == chunk_size) {
                flag |= CHUNK_END;
                if (m <= CHUNK_SIZE) {
                    // Only one block, so we can just hash it directly.
                    flag |= ROOT;
                }
            }
            compress_in_place(cv, msg_block, counter, BLOCK_SIZE, flag);
        }
        counter += 1;
        // Count trailing zeros in counter.
        uint32_t n_merges = std::countr_zero(counter);
        const bool is_last_chunk = chunk_offset + CHUNK_SIZE >= m;
        for (uint32_t i = 0; i < n_merges; i++) {
            // Pop stack to first of msg_block
            next_stack--;
            for (size_t j = 0; j < 8; j++) {
                msg_block[j] = cv_stack[next_stack][j];
            }
            // Copy current CV to second half of msg_block
            for (size_t j = 0; j < 8; j++) {
                msg_block[j + 8] = cv[j];
            }
            reset_cv();
            uint8_t flag = PARENT;
            if (i == n_merges - 1 && is_last_chunk) {
                // Last merge on last chunk, mark as root.
                flag |= ROOT;
            }
            compress_in_place(cv, msg_block, 0, BLOCK_SIZE, flag);
        }
        if (!is_last_chunk) {
            // Push CV to stack and reset.
            for (size_t j = 0; j < 8; j++) {
                cv_stack[next_stack][j] = cv[j];
            }
            next_stack++;
            reset_cv();
        }
    }
    for (size_t i = 0; i < 8; i++) {
        reinterpret_cast<uint32_t *>(out)[i] = cv[i];
    }
}

// NOLINTEND(*-avoid-c-arrays, *-reinterpret-cast, *-avoid-magic-numbers,
// *-bounds-array-to-pointer-decay)

#undef ALWAYS_INLINE

} // namespace blake3

#ifndef BLAKE3_OUT_LEN
constexpr size_t BLAKE3_OUT_LEN = 32; // For compatibility with BLAKE3.
#endif

} // namespace