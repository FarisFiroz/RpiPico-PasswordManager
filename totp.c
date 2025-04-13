#include "totp.h"
#include "base32.h"
#include "sha1.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CFG_TUD_HID 1


int totp(uint64_t current_time, const char *base32key, int step_secs, int digits,
         char *otp, size_t otp_size, int *time_remaining) {
    uint8_t key_bytes[64];  // Buffer for decoded secret key.
    int key_len = base32_decode(base32key, key_bytes, sizeof(key_bytes));
    if (key_len <= 0) {
        return -1;
    }

    // Calculate time counter (steps since epoch).
    uint64_t counter = current_time / step_secs;
    uint8_t counter_bytes[8];
    for (int i = 0; i < 8; i++) {
        counter_bytes[7 - i] = (uint8_t)(counter >> (8 * i));
    }

    // Compute HMAC-SHA1 of the time counter using the secret key.
    uint8_t hmac_result[20];
    hmac_sha1(key_bytes, key_len, counter_bytes, sizeof(counter_bytes), hmac_result);

    // Dynamic truncation to extract a 31-bit code.
    int offset = hmac_result[19] & 0x0F;
    uint32_t code = ((hmac_result[offset] & 0x7F) << 24) |
                    ((hmac_result[offset+1] & 0xFF) << 16) |
                    ((hmac_result[offset+2] & 0xFF) << 8) |
                    (hmac_result[offset+3] & 0xFF);

    // Compute OTP value modulo 10^digits.
    uint32_t divisor = 1;
    for (int i = 0; i < digits; i++) {
        divisor *= 10;
    }
    uint32_t otp_value = code % divisor;

    // Format OTP as a zero-padded string.
    snprintf(otp, otp_size, "%0*u", digits, otp_value);

    // Calculate seconds until OTP expires.
    *time_remaining = step_secs - (current_time % step_secs);
    return 0;
}
