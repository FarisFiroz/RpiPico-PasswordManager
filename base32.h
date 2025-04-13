#ifndef BASE32_H
#define BASE32_H

#include <stddef.h>
#include <stdint.h>

// Decodes a Base32-encoded string into bytes.
// 'encoded' is the Base32 string, 'output' is the destination buffer with size 'out_max'.
// Returns the number of decoded bytes, or -1 on error.
int base32_decode(const char *encoded, uint8_t *output, size_t out_max);

#endif // BASE32_H
