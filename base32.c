#include "base32.h"
#include <string.h>

int base32_decode(const char *encoded, uint8_t *output, size_t out_max) {
    size_t encoded_len = strlen(encoded);
    size_t out_index = 0;
    int bits = 0;
    uint32_t bit_buffer = 0;

    for (size_t i = 0; i < encoded_len; i++) {
        char c = encoded[i];
        // Skip padding and whitespace.
        if (c == '=' || c == ' ' || c == '\t' || c == '\n' || c == '\r')
            continue;

        int val;
        if (c >= 'A' && c <= 'Z') {
            val = c - 'A';
        } else if (c >= '2' && c <= '7') {
            val = c - '2' + 26;
        } else if (c >= 'a' && c <= 'z') {
            val = c - 'a';
        } else {
            return -1;
        }

        bit_buffer = (bit_buffer << 5) | (val & 0x1F);
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            if (out_index >= out_max)
                return -1;
            output[out_index++] = (uint8_t)(bit_buffer >> bits);
            bit_buffer &= ((1 << bits) - 1);
        }
    }
    return out_index;
}
