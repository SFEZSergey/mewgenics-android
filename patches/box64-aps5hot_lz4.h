// SPDX-License-Identifier: Zlib
// ARM64 safe LZ4 block decoder for the Mewgenics app.elf decoder entry.
#ifndef APS5HOT_LZ4_H
#define APS5HOT_LZ4_H

#include <stdint.h>
#include <string.h>
#include "emu/x64emu_private.h"
#include "regs.h"

static int Aps5Lz4DecompressSafe(const uint8_t* src, uint8_t* dst, int src_size, int dst_capacity)
{
    if (!src || src_size < 0 || dst_capacity < 0 || (!dst && dst_capacity != 0))
        return -1;
    if (dst_capacity == 0) {
        if (src_size == 0)
            return 0;
        return src_size == 1 && src[0] == 0 ? 0 : -1;
    }
    if (src_size == 0)
        return -1;

    size_t input = 0;
    size_t output = 0;
    const size_t input_size = (size_t)src_size;
    const size_t output_capacity = (size_t)dst_capacity;

    while (input < input_size) {
        const uint8_t token = src[input++];
        size_t literal_length = token >> 4;
        if (literal_length == 15) {
            uint8_t extension;
            do {
                if (input == input_size || literal_length > SIZE_MAX - 255)
                    return -1;
                extension = src[input++];
                literal_length += extension;
            } while (extension == 255);
        }
        if (literal_length > input_size - input || literal_length > output_capacity - output)
            return -1;
        memcpy(dst + output, src + input, literal_length);
        input += literal_length;
        output += literal_length;

        // The last sequence may contain only literals.
        if (input == input_size)
            return (int)output;
        if (input_size - input < 2)
            return -1;

        const size_t offset = (size_t)src[input] | (size_t)src[input + 1] << 8;
        input += 2;
        if (offset == 0 || offset > output)
            return -1;

        size_t match_length = (token & 0x0f) + 4;
        if ((token & 0x0f) == 15) {
            uint8_t extension;
            do {
                if (input == input_size || match_length > SIZE_MAX - 255)
                    return -1;
                extension = src[input++];
                match_length += extension;
            } while (extension == 255);
        }
        if (match_length > output_capacity - output)
            return -1;

        // Copy a non-overlapping seed, then grow it by doubling for repeated matches.
        if (offset >= match_length) {
            memcpy(dst + output, dst + output - offset, match_length);
        } else {
            memcpy(dst + output, dst + output - offset, offset);
            size_t copied = offset;
            while (copied < match_length) {
                size_t amount = copied;
                if (amount > match_length - copied)
                    amount = match_length - copied;
                memcpy(dst + output + copied, dst + output, amount);
                copied += amount;
            }
        }
        output += match_length;
    }
    return (int)output;
}

static void Aps5Wrap_iFppii(x64emu_t* emu, uintptr_t fcn)
{
    (void)emu;
    (void)fcn;
    R_RAX = (uint32_t)Aps5Lz4DecompressSafe((const uint8_t*)R_RDI, (uint8_t*)R_RSI,
                                            (int32_t)R_RDX, (int32_t)R_RCX);
}

#endif
