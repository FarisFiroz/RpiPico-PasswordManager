#ifndef TOTP_H
#define TOTP_H

#include <stddef.h>
#include <stdint.h>

// Generates a Time-based One-Time Password (TOTP).
// Parameters:
//   current_time   - current Unix time in seconds.
//   base32key      - Base32-encoded secret key.
//   step_secs      - time step in seconds (e.g., 30).
//   digits         - number of digits for the OTP (e.g., 6).
//   otp            - output buffer for the OTP string.
//   otp_size       - size of the output buffer.
//   time_remaining - pointer to store seconds until the OTP expires.
// Returns 0 on success, or -1 on error.
int totp(uint64_t current_time, const char *base32key, int step_secs, int digits,
         char *otp, size_t otp_size, int *time_remaining);

#endif // TOTP_H
