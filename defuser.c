#include "defuser.h"

#include <inttypes.h>
#include <stdio.h>

#define MAX_FIELDS 8

/*
 * 把 values 中的 count 个整数写成一行“空格分隔的无符号十进制整数”，存入 out。
 * 成功返回 0；count 为 0（还没有实现）、snprintf 出错或结果被截断时返回非零。
 */
static int write_answer(char *out, size_t cap, const uint32_t *values, size_t count) {
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        int size = snprintf(out + used, cap - used, "%s%" PRIu32, i ? " " : "", values[i]);
        if (size < 0 || (size_t)size >= cap - used) return 1;
        used += (size_t)size;
    }
    return count == 0;
}

int solve_phase_1(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 1;
    uint32_t H = (uint32_t)(challenge >> 32);
    uint32_t M = (uint32_t)(challenge >> 16);
    uint32_t L = (uint32_t)challenge;

    uint32_t A = 5u * (H - M) + 0x6c41u;
    uint32_t B = (5u * L + H) ^ 0x3c26u;
    values[0] = (A ^ 0xa906u) + 2u * B;
    // TODO: 根据 phase_01 的汇编，用 challenge 算出口令中的各个整数，依次存入 values，并把 count 改为整数个数。
    return write_answer(out, cap, values, count);
}

int solve_phase_2(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 6;
     values[0] = (
        ((uint32_t)(challenge >> 17) * 2u) & 0xffu
    ) + 29u;
     for (uint32_t i = 1; i < 6; i++) {
        uint32_t shift = 10u + 5u * (i - 1u);
        uint32_t t = ((uint32_t)(challenge >> shift)) & 0x1fu;
        uint32_t x = (t + i) ^ values[i - 1];
        values[i] = x * 5u;
    }
    // TODO: 根据 phase_02 的汇编，用 challenge 算出口令中的各个整数，依次存入 values，并把 count 改为整数个数。
    return write_answer(out, cap, values, count);
}

int solve_phase_3(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 2;
    uint32_t x = (uint32_t)challenge & 7u;
    uint32_t y = (uint32_t)(challenge >> 32)
               ^ (uint32_t)(challenge >> 3);
    uint32_t result = 0;
    switch (x) {
        case 0:
            result = (y ^ 0x7259u) + (y >> 1);
            break;

        case 1:
            result = 5u * y + 0x2d866u;
            break;

        case 2:
            result = (2u * y) ^ 0x1da2u;
            break;

        case 3:
            result = 3u * y + 0xb1d8u;
            break;

        case 4:
            result = 5u * y + 0x35476u;
            break;

        case 5:
            result = (y ^ 0x50eeu) + (y >> 1);
            break;

        case 6:
            result = 5u * y + 0x6062u;
            break;

        case 7:
            result = (4u * y) ^ 0x1956u;
            break;
    }

    values[0] = x;
    values[1] = result;
    // TODO: 根据 phase_03 的汇编，用 challenge 算出口令中的各个整数，依次存入 values，并把 count 改为整数个数。
    return write_answer(out, cap, values, count);
}

static uint32_t p4_walk_answer(
    uint32_t target,
    uint32_t low,
    uint32_t high,
    uint32_t context
) {
    uint32_t mid = low + ((high - low) >> 1);

    if (target == mid) {
        return (mid + 30u) ^ context;
    }

    uint32_t mixed = mid + context;

    if (target < mid) {
        uint32_t child = p4_walk_answer(
            target, low, mid - 1u, context
        );

        return (child * 5u) ^ mixed ^ 0x15u;
    }

    else {
        uint32_t child = p4_walk_answer(
            target, mid + 1u, high, context
        );

        return (child * 5u) ^ mixed ^ 1u;
    }
}

int solve_phase_4(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 1;
    
    uint32_t target = (uint32_t)challenge & 0x7fu;
    uint32_t context =
        (uint32_t)(challenge >> 7) ^
        (uint32_t)(challenge >> 32);
        values[0] = p4_walk_answer(
        target, 0u, 127u, context
    );
    // TODO: 根据 phase_04 的汇编，用 challenge 算出口令中的各个整数，依次存入 values，并把 count 改为整数个数。
    return write_answer(out, cap, values, count);
}

int solve_phase_5(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 2;
    static const uint32_t node_a[11] = {
        0x125u, 0x526u, 0x124u, 0x1c31u,
        0xe958u, 0x5482u, 0x21c6u, 0x68e7u,
        0x2933u, 0x31c3u, 0x178du
    };
    static const uint32_t node_b[11] = {
        0x231u, 0x970u, 0x362u, 0x860u,
        0xb50u, 0xa59u, 0x60u, 0x127u,
        0x74au, 0x523u, 0x43du
    };
    static const uint32_t node_next[11] = {
        5u, 9u, 0u, 2u, 1u, 6u,
        7u, 8u, 3u, 0u, 4u
    };

    static const uint32_t heads[8] = {
        10u, 4u, 1u, 9u, 0u, 5u, 6u, 7u
    };

    uint32_t index = (uint32_t)challenge & 7u;

    uint32_t steps = ((uint32_t)(challenge >> 3)) & 3u;

    uint32_t temp =
        (uint32_t)(challenge >> 6) ^
        (uint32_t)(challenge >> 32) ^
        0x19fu;

    uint32_t node = heads[index];

    uint32_t b = node_b[node];

    uint32_t result = 3u * temp + node_a[node];
    result ^= b;
    for (uint32_t i = 0; i < steps; i++) {
        node = node_next[node];

        b = node_b[node];

        result = 3u * result + node_a[node];
        result ^= b;
    }
    values[0] = result;
    values[1] = b;
    // TODO: 根据 phase_05 的汇编，用 challenge 算出口令中的各个整数，依次存入 values，并把 count 改为整数个数。
    return write_answer(out, cap, values, count);
}

static uint32_t p6_rol32(uint32_t x, unsigned n) {
    return (x << n) | (x >> (32u - n));
}

static uint32_t p6_ror32(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32u - n));
}
static uint32_t p6_apply(uint32_t index, uint32_t x) {
    switch (index) {
        case 0:
        case 1:
        case 2:
            return (x + 0x8d5au) ^ (x >> 7);

        case 3:
            return p6_ror32(x, 5);

        case 4:
            return p6_rol32(x, 3) ^ 0x11c1u;

        case 5:
            return ((x << 5) + 0x5f40u) ^ (3u * x);

        case 6:
            return ((x >> 1) ^ x) + 0x6b3cu;

        case 7:
            return (x ^ 0x3be5u) + (x << 7);

        default:
            return 0;
    }
}


int solve_phase_6(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 1;

    uint32_t y =
        (uint32_t)(challenge >> 6) ^
        (uint32_t)(challenge >> 32);

    uint32_t index1 = (uint32_t)challenge & 7u;
    uint32_t index2 = ((uint32_t)challenge >> 3) & 7u;

    uint32_t first = p6_apply(index1, y);

    uint32_t second_input = first ^ 0xfe1du;

    uint32_t second = p6_apply(index2, second_input);

    values[0] = second + 0x30a6u;

    return write_answer(out, cap, values, count);
}

int solve_secret(uint64_t challenge, uint64_t context,
                 char *out, size_t cap) {

    uint32_t c_low = (uint32_t)challenge;
    uint32_t c_high = (uint32_t)(challenge >> 32);
    uint32_t k_low = (uint32_t)context;
    uint32_t k_high = (uint32_t)(context >> 32);
    uint32_t S = c_high ^ c_low;
    uint32_t T = k_high ^ k_low;
    uint32_t X = S ^ T;
    uint32_t v0 = k_low & 3u;
    uint32_t mixed =
        ((X >> 1) ^ 0x6c9du) +
        (X ^ 0x2aacu);

    uint32_t v1 =
        mixed ^ ((v0 * 0x2aacu) ^ (uint32_t)(context >> 16));

    uint32_t v2 =
        v1 ^ ((S >> 1) + T + v0 + 0x6c9du);

    int n = snprintf(
        out,
        cap,
        "AAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAA"
        "OPEN"
        " %" PRIu32 " %" PRIu32 " %" PRIu32,
        v0, v1, v2
    );

    return n < 0 || (size_t)n >= cap;
}

    // TODO（选做）: 根据 secret_gate、secret_copy 和 secret_phase 的汇编构造完整的第七行，写入 out 后返回 0。
    // 第七行不一定只有整数，可以直接用 snprintf 写入；还没完成时保持返回 1，测试会显示 not attempted。


