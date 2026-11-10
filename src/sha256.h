/* sha256.h — SHA-256, HMAC-SHA256 e PBKDF2-HMAC-SHA256 (só o necessário
   para guardar a senha do administrador sem depender de biblioteca). */
#ifndef RAQ_SHA256_H
#define RAQ_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t h[8];
    uint64_t total;
    unsigned char bloco[64];
    size_t   usado;
} Sha256;

void sha256_inicia(Sha256 *c);
void sha256_adiciona(Sha256 *c, const void *dados, size_t n);
void sha256_finaliza(Sha256 *c, unsigned char saida[32]);
void sha256(const void *dados, size_t n, unsigned char saida[32]);

/* PBKDF2-HMAC-SHA256 com saída de 32 bytes (um bloco). */
void pbkdf2_sha256(const unsigned char *senha, size_t nsenha,
                   const unsigned char *sal, size_t nsal,
                   unsigned long iteracoes, unsigned char saida[32]);

#endif /* RAQ_SHA256_H */
