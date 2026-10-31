/*
 * SHA-256 implementation in C
 * Self-contained, zero-dependency.
 */
#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t  data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len);
void sha256_final(SHA256_CTX *ctx, uint8_t hash[]);

/* Helper to compute hex SHA-256 for a string (output must be at least 65 bytes) */
void sha256_hash_string(const char *str, char *output_hex);

#endif /* SHA256_H */
