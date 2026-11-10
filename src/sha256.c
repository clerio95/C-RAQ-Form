#include "sha256.h"

#include <string.h>

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void processa(Sha256 *c, const unsigned char *b)
{
    uint32_t w[64], a, bb, cc, d, e, f, g, h;
    int i;

    for (i = 0; i < 16; i++)
        w[i] = (uint32_t)b[4 * i] << 24 | (uint32_t)b[4 * i + 1] << 16
             | (uint32_t)b[4 * i + 2] << 8 | (uint32_t)b[4 * i + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = c->h[0]; bb = c->h[1]; cc = c->h[2]; d = c->h[3];
    e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];

    for (i = 0; i < 64; i++) {
        uint32_t S1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);
        uint32_t mj = (a & bb) ^ (a & cc) ^ (bb & cc);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = bb; bb = a; a = t1 + t2;
    }

    c->h[0] += a; c->h[1] += bb; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f;  c->h[6] += g;  c->h[7] += h;
}

void sha256_inicia(Sha256 *c)
{
    static const uint32_t ini[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    memcpy(c->h, ini, sizeof ini);
    c->total = 0;
    c->usado = 0;
}

void sha256_adiciona(Sha256 *c, const void *dados, size_t n)
{
    const unsigned char *p = (const unsigned char *)dados;
    c->total += n;
    while (n > 0) {
        size_t cabe = 64 - c->usado;
        if (cabe > n) cabe = n;
        memcpy(c->bloco + c->usado, p, cabe);
        c->usado += cabe;
        p += cabe;
        n -= cabe;
        if (c->usado == 64) { processa(c, c->bloco); c->usado = 0; }
    }
}

void sha256_finaliza(Sha256 *c, unsigned char saida[32])
{
    uint64_t bits = c->total * 8;
    unsigned char pad = 0x80, zero = 0, tam[8];
    int i;

    sha256_adiciona(c, &pad, 1);
    while (c->usado != 56) sha256_adiciona(c, &zero, 1);
    for (i = 0; i < 8; i++) tam[i] = (unsigned char)(bits >> (56 - 8 * i));
    sha256_adiciona(c, tam, 8);

    for (i = 0; i < 8; i++) {
        saida[4 * i]     = (unsigned char)(c->h[i] >> 24);
        saida[4 * i + 1] = (unsigned char)(c->h[i] >> 16);
        saida[4 * i + 2] = (unsigned char)(c->h[i] >> 8);
        saida[4 * i + 3] = (unsigned char)(c->h[i]);
    }
}

void sha256(const void *dados, size_t n, unsigned char saida[32])
{
    Sha256 c;
    sha256_inicia(&c);
    sha256_adiciona(&c, dados, n);
    sha256_finaliza(&c, saida);
}

/* HMAC com as chaves interna/externa já preparadas, para não refazer o
   preparo da chave a cada uma das milhares de iterações do PBKDF2. */
typedef struct { Sha256 interno, externo; } Hmac;

static void hmac_prepara(Hmac *m, const unsigned char *chave, size_t n)
{
    unsigned char k[64], ipad[64], opad[64];
    int i;

    memset(k, 0, sizeof k);
    if (n > 64) sha256(chave, n, k);
    else if (n) memcpy(k, chave, n);

    for (i = 0; i < 64; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }
    sha256_inicia(&m->interno); sha256_adiciona(&m->interno, ipad, 64);
    sha256_inicia(&m->externo); sha256_adiciona(&m->externo, opad, 64);
}

static void hmac_calcula(const Hmac *m, const unsigned char *a, size_t na,
                         const unsigned char *b, size_t nb,
                         unsigned char saida[32])
{
    Sha256 c = m->interno;
    unsigned char meio[32];
    sha256_adiciona(&c, a, na);
    if (nb) sha256_adiciona(&c, b, nb);
    sha256_finaliza(&c, meio);

    c = m->externo;
    sha256_adiciona(&c, meio, 32);
    sha256_finaliza(&c, saida);
}

void pbkdf2_sha256(const unsigned char *senha, size_t nsenha,
                   const unsigned char *sal, size_t nsal,
                   unsigned long iteracoes, unsigned char saida[32])
{
    static const unsigned char bloco1[4] = { 0, 0, 0, 1 };
    Hmac m;
    unsigned char u[32];
    unsigned long i;
    int k;

    hmac_prepara(&m, senha, nsenha);
    hmac_calcula(&m, sal, nsal, bloco1, 4, u);
    memcpy(saida, u, 32);
    for (i = 1; i < iteracoes; i++) {
        hmac_calcula(&m, u, 32, NULL, 0, u);
        for (k = 0; k < 32; k++) saida[k] ^= u[k];
    }
}
