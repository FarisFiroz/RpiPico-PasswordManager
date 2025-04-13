#ifndef SHA1_H
#define SHA1_H

#include <stddef.h>
#include <stdint.h>

// Computes SHA-1 hash of the input message.
// 'msg' is the input data of length 'len' bytes.
// 'digest' must point to a buffer of at least 20 bytes.
void sha1(const uint8_t *msg, size_t len, uint8_t *digest);

// Computes HMAC-SHA1 of a message with the given key.
// 'key' and 'msg' have lengths 'key_len' and 'msg_len' respectively.
// The resulting 20-byte hash is stored in 'digest'.
void hmac_sha1(const uint8_t *key, size_t key_len,
               const uint8_t *msg, size_t msg_len,
               uint8_t *digest);

#endif // SHA1_H
