#include "sha1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Helper: Left-rotate a 32-bit integer 'value' by 'count' bits.
static uint32_t leftrotate(uint32_t value, unsigned int count) {
return (value << count) | (value >> (32 - count));
}

void sha1(const uint8_t *msg, size_t len, uint8_t *digest) {
// Initial hash constants.
uint32_t h0 = 0x67452301;
uint32_t h1 = 0xEFCDAB89;
uint32_t h2 = 0x98BADCFE;
uint32_t h3 = 0x10325476;
uint32_t h4 = 0xC3D2E1F0;

uint64_t bit_len = len * 8;
size_t new_len = len + 1;
while (new_len % 64 != 56)
new_len++;
new_len += 8;

uint8_t *msg_padded = (uint8_t*)calloc(new_len, sizeof(uint8_t));
if (!msg_padded) {
fprintf(stderr, "Memory allocation error\n");
exit(1);
}
memcpy(msg_padded, msg, len);
msg_padded[len] = 0x80; // Append the '1' bit

// Append the original message length (in bits) as a 64-bit big-endian integer.
for (int i = 0; i < 8; i++) {
msg_padded[new_len - 1 - i] = (uint8_t)((bit_len >> (8 * i)) & 0xFF);
}

size_t chunk_count = new_len / 64;
for (size_t i = 0; i < chunk_count; i++) {
uint32_t w[80];
// Break chunk into sixteen 32-bit big-endian words.
for (int j = 0; j < 16; j++) {
size_t index = i * 64 + j * 4;
w[j] = ((uint32_t)msg_padded[index] << 24) |
((uint32_t)msg_padded[index+1] << 16) |
((uint32_t)msg_padded[index+2] << 8) |
((uint32_t)msg_padded[index+3]);
}
// Extend the sixteen words into eighty words.
for (int j = 16; j < 80; j++) {
w[j] = leftrotate(w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16], 1);
}

// Initialize working variables.
uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
for (int j = 0; j < 80; j++) {
uint32_t f, k;
if (j < 20) {
f = (b & c) | ((~b) & d);
k = 0x5A827999;
} else if (j < 40) {
f = b ^ c ^ d;
k = 0x6ED9EBA1;
} else if (j < 60) {
f = (b & c) | (b & d) | (c & d);
k = 0x8F1BBCDC;
} else {
f = b ^ c ^ d;
k = 0xCA62C1D6;
}
uint32_t temp = leftrotate(a, 5) + f + e + k + w[j];
e = d;
d = c;
c = leftrotate(b, 30);
b = a;
a = temp;
}
// Add the chunk's hash to the result.
h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
}
free(msg_padded);

// Produce the final hash value in big-endian format.
digest[0] = (uint8_t)((h0 >> 24) & 0xFF);
digest[1] = (uint8_t)((h0 >> 16) & 0xFF);
digest[2] = (uint8_t)((h0 >> 8) & 0xFF);
digest[3] = (uint8_t)(h0 & 0xFF);
digest[4] = (uint8_t)((h1 >> 24) & 0xFF);
digest[5] = (uint8_t)((h1 >> 16) & 0xFF);
digest[6] = (uint8_t)((h1 >> 8) & 0xFF);
digest[7] = (uint8_t)(h1 & 0xFF);
digest[8] = (uint8_t)((h2 >> 24) & 0xFF);
digest[9] = (uint8_t)((h2 >> 16) & 0xFF);
digest[10] = (uint8_t)((h2 >> 8) & 0xFF);
digest[11] = (uint8_t)(h2 & 0xFF);
digest[12] = (uint8_t)((h3 >> 24) & 0xFF);
digest[13] = (uint8_t)((h3 >> 16) & 0xFF);
digest[14] = (uint8_t)((h3 >> 8) & 0xFF);
digest[15] = (uint8_t)(h3 & 0xFF);
digest[16] = (uint8_t)((h4 >> 24) & 0xFF);
digest[17] = (uint8_t)((h4 >> 16) & 0xFF);
digest[18] = (uint8_t)((h4 >> 8) & 0xFF);
digest[19] = (uint8_t)(h4 & 0xFF);
}

void hmac_sha1(const uint8_t *key, size_t key_len,
const uint8_t *msg, size_t msg_len,
uint8_t *digest) {
const size_t block_size = 64;
uint8_t key_block[64];
uint8_t ipad[64];
uint8_t opad[64];

// If key is longer than block_size, hash it first.
if (key_len > block_size) {
sha1(key, key_len, key_block);
memset(key_block + 20, 0, block_size - 20);
} else {
memcpy(key_block, key, key_len);
memset(key_block + key_len, 0, block_size - key_len);
}

// Create the inner and outer padded keys.
for (size_t i = 0; i < block_size; i++) {
ipad[i] = key_block[i] ^ 0x36;
opad[i] = key_block[i] ^ 0x5C;
}

// Compute inner hash: SHA1(ipad || msg)
uint8_t *inner_data = malloc(block_size + msg_len);
if (!inner_data) {
fprintf(stderr, "Memory allocation error\n");
exit(1);
}
memcpy(inner_data, ipad, block_size);
memcpy(inner_data + block_size, msg, msg_len);
uint8_t inner_hash[20];
sha1(inner_data, block_size + msg_len, inner_hash);
free(inner_data);

// Compute outer hash: SHA1(opad || inner_hash)
uint8_t outer_data[64 + 20];
memcpy(outer_data, opad, block_size);
memcpy(outer_data + block_size, inner_hash, 20);
sha1(outer_data, block_size + 20, digest);
}

