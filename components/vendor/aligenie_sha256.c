/*
 * SHA-256 hash implementation and interface functions
 * Copyright (c) 2003-2007, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <stddef.h>
#include <string.h>

#define SHA256_MAC_LEN 32

 static void ag_sha256_vector(size_t num_elem, const unsigned char *addr[], const size_t *len,
                   unsigned char *mac);

/**
 * hmac_sha256_vector - HMAC-SHA256 over data vector (RFC 2104)
 * @key: Key for HMAC operations
 * @key_len: Length of the key in bytes
 * @num_elem: Number of elements in the data vector
 * @addr: Pointers to the data areas
 * @len: Lengths of the data blocks
 * @mac: Buffer for the hash (32 bytes)
 */
static void ag_hmac_sha256_vector(const unsigned char *key, size_t key_len, size_t num_elem,
                        const unsigned char *addr[], const size_t *len, unsigned char *mac)
{
    unsigned char k_pad[64]; /* padding - key XORd with ipad/opad */
    unsigned char tk[32];
    const unsigned char *_addr[6];
    size_t _len[6], i;

    if (num_elem > 5) {
        /*
         * Fixed limit on the number of fragments to avoid having to
         * allocate memory (which could fail).
         */
        return;
    }

    /* if key is longer than 64 bytes reset it to key = SHA256(key) */
    if (key_len > 64) {
        ag_sha256_vector(1, &key, &key_len, tk);
        key = tk;
        key_len = 32;
    }

    /* the HMAC_SHA256 transform looks like:
     *
     * SHA256(K XOR opad, SHA256(K XOR ipad, text))
     *
     * where K is an n byte key
     * ipad is the byte 0x36 repeated 64 times
     * opad is the byte 0x5c repeated 64 times
     * and text is the data being protected */

    /* start out by storing key in ipad */
    memset(k_pad, 0, sizeof(k_pad));
    memcpy(k_pad, key, key_len);
    /* XOR key with ipad values */
    for (i = 0; i < 64; i++) {
        k_pad[i] ^= 0x36;
    }

    /* perform inner SHA256 */
    _addr[0] = k_pad;
    _len[0] = 64;
    for (i = 0; i < num_elem; i++) {
        _addr[i + 1] = addr[i];
        _len[i + 1] = len[i];
    }
    ag_sha256_vector(1 + num_elem, _addr, _len, mac);

    memset(k_pad, 0, sizeof(k_pad));
    memcpy(k_pad, key, key_len);
    /* XOR key with opad values */
    for (i = 0; i < 64; i++) {
        k_pad[i] ^= 0x5c;
    }

    /* perform outer SHA256 */
    _addr[0] = k_pad;
    _len[0] = 64;
    _addr[1] = mac;
    _len[1] = SHA256_MAC_LEN;
    ag_sha256_vector(2, _addr, _len, mac);
}


/**
 * wps_hmac_sha256 - HMAC-SHA256 over data buffer (RFC 2104)
 * @key: Key for HMAC operations
 * @key_len: Length of the key in bytes
 * @data: Pointers to the data area
 * @data_len: Length of the data area
 * @mac: Buffer for the hash (20 bytes)
 */
void ag_hmac_sha256(const unsigned char *key, size_t key_len, const unsigned char *data,
                     size_t data_len, unsigned char *mac)
{
    ag_hmac_sha256_vector(key, key_len, 1, &data, &data_len, mac);
}


struct sha256_state {
    unsigned long long length;
    unsigned int state[8], curlen;
    unsigned char buf[64];
};

static void ag_sha256_init(struct sha256_state *md);
static int ag_sha256_process(struct sha256_state *md, const unsigned char *in,
                          unsigned long inlen);
static int ag_sha256_done(struct sha256_state *md, unsigned char *out);


/**
 * sha256_vector - SHA256 hash for data vector
 * @num_elem: Number of elements in the data vector
 * @addr: Pointers to the data areas
 * @len: Lengths of the data blocks
 * @mac: Buffer for the hash
 */
void ag_sha256_vector(size_t num_elem, const unsigned char *addr[], const size_t *len,
                   unsigned char *mac)
{
    struct sha256_state ctx;
    size_t i;

    ag_sha256_init(&ctx);
    for (i = 0; i < num_elem; i++) {
        ag_sha256_process(&ctx, addr[i], len[i]);
    }
    ag_sha256_done(&ctx, mac);
}


/* ===== start - public domain SHA256 implementation ===== */

/* This is based on SHA256 implementation in LibTomCrypt that was released into
 * public domain by Tom St Denis. */

/* the K array */
static const unsigned long K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
    0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
    0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
    0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
    0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
    0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
    0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
    0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
    0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
    0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};


/* Various logical functions */
#define RORc(x, y) \
( ((((unsigned long) (x) & 0xFFFFFFFFUL) >> (unsigned long) ((y) & 31)) | \
   ((unsigned long) (x) << (unsigned long) (32 - ((y) & 31)))) & 0xFFFFFFFFUL)
#define Ch(x,y,z)       (z ^ (x & (y ^ z)))
#define Maj(x,y,z)      (((x | y) & z) | (x & y))
#define S(x, n)         RORc((x), (n))
#define R(x, n)         (((x)&0xFFFFFFFFUL)>>(n))
#define Sigma0(x)       (S(x, 2) ^ S(x, 13) ^ S(x, 22))
#define Sigma1(x)       (S(x, 6) ^ S(x, 11) ^ S(x, 25))
#define Gamma0(x)       (S(x, 7) ^ S(x, 18) ^ R(x, 3))
#define Gamma1(x)       (S(x, 17) ^ S(x, 19) ^ R(x, 10))
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif
#define WPA_GET_BE32(a) ((((unsigned int) (a)[0]) << 24) | \
                        (((unsigned int) (a)[1]) << 16) | \
                        (((unsigned int) (a)[2]) << 8) | \
                        ((unsigned int) (a)[3]))

#define WPA_PUT_BE32(a, val)                    \
    do {                            \
        (a)[0] = (unsigned char) ((((unsigned int) (val)) >> 24) & 0xff);   \
        (a)[1] = (unsigned char) ((((unsigned int) (val)) >> 16) & 0xff);   \
        (a)[2] = (unsigned char) ((((unsigned int) (val)) >> 8) & 0xff);    \
        (a)[3] = (unsigned char) (((unsigned int) (val)) & 0xff);       \
    } while (0)

#define WPA_PUT_BE64(a, val)                \
        do {                        \
            (a)[0] = (unsigned char) (((unsigned long long) (val)) >> 56);    \
            (a)[1] = (unsigned char) (((unsigned long long) (val)) >> 48);    \
            (a)[2] = (unsigned char) (((unsigned long long) (val)) >> 40);    \
            (a)[3] = (unsigned char) (((unsigned long long) (val)) >> 32);    \
            (a)[4] = (unsigned char) (((unsigned long long) (val)) >> 24);    \
            (a)[5] = (unsigned char) (((unsigned long long) (val)) >> 16);    \
            (a)[6] = (unsigned char) (((unsigned long long) (val)) >> 8); \
            (a)[7] = (unsigned char) (((unsigned long long) (val)) & 0xff);   \
        } while (0)



/* compress 512-bits */
static int ag_sha256_compress(struct sha256_state *md, unsigned char *buf)
{
    unsigned int S[8], W[64], t0, t1;
    unsigned int t;
    int i;

    /* copy state into S */
    for (i = 0; i < 8; i++) {
        S[i] = md->state[i];
    }

    /* copy the state into 512-bits into W[0..15] */
    for (i = 0; i < 16; i++) {
        W[i] = WPA_GET_BE32(buf + (4 * i));
    }

    /* fill W[16..63] */
    for (i = 16; i < 64; i++) {
        W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) +
               W[i - 16];
    }

    /* Compress */
#define RND(a,b,c,d,e,f,g,h,i)                          \
    t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i]; \
    t1 = Sigma0(a) + Maj(a, b, c);          \
    d += t0;                    \
    h  = t0 + t1;

    for (i = 0; i < 64; ++i) {
        RND(S[0], S[1], S[2], S[3], S[4], S[5], S[6], S[7], i);
        t = S[7];
        S[7] = S[6];
        S[6] = S[5];
        S[5] = S[4];
        S[4] = S[3];
        S[3] = S[2];
        S[2] = S[1];
        S[1] = S[0];
        S[0] = t;
    }

    /* feedback */
    for (i = 0; i < 8; i++) {
        md->state[i] = md->state[i] + S[i];
    }
    return 0;
}


/* Initialize the hash state */
static void ag_sha256_init(struct sha256_state *md)
{
    md->curlen = 0;
    md->length = 0;
    md->state[0] = 0x6A09E667UL;
    md->state[1] = 0xBB67AE85UL;
    md->state[2] = 0x3C6EF372UL;
    md->state[3] = 0xA54FF53AUL;
    md->state[4] = 0x510E527FUL;
    md->state[5] = 0x9B05688CUL;
    md->state[6] = 0x1F83D9ABUL;
    md->state[7] = 0x5BE0CD19UL;
}

/**
   Process a block of memory though the hash
   @param md     The hash state
   @param in     The data to hash
   @param inlen  The length of the data (octets)
   @return CRYPT_OK if successful
*/
static int ag_sha256_process(struct sha256_state *md, const unsigned char *in,
                          unsigned long inlen)
{
    unsigned long n;
#define block_size 64

    if (md->curlen > sizeof(md->buf)) {
        return -1;
    }

    while (inlen > 0) {
        if (md->curlen == 0 && inlen >= block_size) {
            if (ag_sha256_compress(md, (unsigned char *) in) < 0) {
                return -1;
            }
            md->length += block_size * 8;
            in += block_size;
            inlen -= block_size;
        } else {
            n = MIN(inlen, (block_size - md->curlen));
            memcpy(md->buf + md->curlen, in, n);
            md->curlen += n;
            in += n;
            inlen -= n;
            if (md->curlen == block_size) {
                if (ag_sha256_compress(md, md->buf) < 0) {
                    return -1;
                }
                md->length += 8 * block_size;
                md->curlen = 0;
            }
        }
    }

    return 0;
}


/**
   Terminate the hash to get the digest
   @param md  The hash state
   @param out [out] The destination of the hash (32 bytes)
   @return CRYPT_OK if successful
*/
static int ag_sha256_done(struct sha256_state *md, unsigned char *out)
{
    int i;

    if (md->curlen >= sizeof(md->buf)) {
        return -1;
    }

    /* increase the length of the message */
    md->length += md->curlen * 8;

    /* append the '1' bit */
    md->buf[md->curlen++] = (unsigned char) 0x80;

    /* if the length is currently above 56 bytes we append zeros
     * then compress.  Then we can fall back to padding zeros and length
     * encoding like normal.
     */
    if (md->curlen > 56) {
        while (md->curlen < 64) {
            md->buf[md->curlen++] = (unsigned char) 0;
        }
        ag_sha256_compress(md, md->buf);
        md->curlen = 0;
    }

    /* pad upto 56 bytes of zeroes */
    while (md->curlen < 56) {
        md->buf[md->curlen++] = (unsigned char) 0;
    }

    /* store length */
    WPA_PUT_BE64(md->buf + 56, md->length);
    ag_sha256_compress(md, md->buf);

    /* copy output */
    for (i = 0; i < 8; i++) {
        WPA_PUT_BE32(out + (4 * i), md->state[i]);
    }

    return 0;
}

/* ===== end - public domain SHA256 implementation ===== */

#define F(x, y, z)   ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z)   ((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z)   ((x) ^ (y) ^ (z))
#define I(x, y, z)   ((y) ^ ((x) | ~(z)))
#define STEP(f, a, b, c, d, x, t, s) \
(a) += f((b), (c), (d)) + (x) + (t); \
(a) = (((a) << (s)) | (((a) & 0xffffffff) >> (32 - (s)))); \
(a) += (b);


typedef unsigned int MD5_u32;

#define SET(n) \
(ctx->block[(n)] = \
(MD5_u32)ptr[(n) * 4] | \
((MD5_u32)ptr[(n) * 4 + 1] << 8) | \
((MD5_u32)ptr[(n) * 4 + 2] << 16) | \
((MD5_u32)ptr[(n) * 4 + 3] << 24))

#define GET(n) (ctx->block[(n)])


typedef struct {
    MD5_u32 lo, hi;
    MD5_u32 a, b, c, d;
    unsigned char buffer[64];
    MD5_u32 block[16];
} ag_md5_ctx;

static const void *body(ag_md5_ctx *ctx, const void *data, unsigned long size){
    const unsigned char *ptr;
    MD5_u32 a, b, c, d;
    MD5_u32 saved_a, saved_b, saved_c, saved_d;
    
    ptr = (const unsigned char*)data;
    
    a = ctx->a;
    b = ctx->b;
    c = ctx->c;
    d = ctx->d;
    
    do {
        saved_a = a;
        saved_b = b;
        saved_c = c;
        saved_d = d;
        
        STEP(F, a, b, c, d, SET(0), 0xd76aa478, 7)
        STEP(F, d, a, b, c, SET(1), 0xe8c7b756, 12)
        STEP(F, c, d, a, b, SET(2), 0x242070db, 17)
        STEP(F, b, c, d, a, SET(3), 0xc1bdceee, 22)
        STEP(F, a, b, c, d, SET(4), 0xf57c0faf, 7)
        STEP(F, d, a, b, c, SET(5), 0x4787c62a, 12)
        STEP(F, c, d, a, b, SET(6), 0xa8304613, 17)
        STEP(F, b, c, d, a, SET(7), 0xfd469501, 22)
        STEP(F, a, b, c, d, SET(8), 0x698098d8, 7)
        STEP(F, d, a, b, c, SET(9), 0x8b44f7af, 12)
        STEP(F, c, d, a, b, SET(10), 0xffff5bb1, 17)
        STEP(F, b, c, d, a, SET(11), 0x895cd7be, 22)
        STEP(F, a, b, c, d, SET(12), 0x6b901122, 7)
        STEP(F, d, a, b, c, SET(13), 0xfd987193, 12)
        STEP(F, c, d, a, b, SET(14), 0xa679438e, 17)
        STEP(F, b, c, d, a, SET(15), 0x49b40821, 22)
        STEP(G, a, b, c, d, GET(1), 0xf61e2562, 5)
        STEP(G, d, a, b, c, GET(6), 0xc040b340, 9)
        STEP(G, c, d, a, b, GET(11), 0x265e5a51, 14)
        STEP(G, b, c, d, a, GET(0), 0xe9b6c7aa, 20)
        STEP(G, a, b, c, d, GET(5), 0xd62f105d, 5)
        STEP(G, d, a, b, c, GET(10), 0x02441453, 9)
        STEP(G, c, d, a, b, GET(15), 0xd8a1e681, 14)
        STEP(G, b, c, d, a, GET(4), 0xe7d3fbc8, 20)
        STEP(G, a, b, c, d, GET(9), 0x21e1cde6, 5)
        STEP(G, d, a, b, c, GET(14), 0xc33707d6, 9)
        STEP(G, c, d, a, b, GET(3), 0xf4d50d87, 14)
        STEP(G, b, c, d, a, GET(8), 0x455a14ed, 20)
        STEP(G, a, b, c, d, GET(13), 0xa9e3e905, 5)
        STEP(G, d, a, b, c, GET(2), 0xfcefa3f8, 9)
        STEP(G, c, d, a, b, GET(7), 0x676f02d9, 14)
        STEP(G, b, c, d, a, GET(12), 0x8d2a4c8a, 20)
        STEP(H, a, b, c, d, GET(5), 0xfffa3942, 4)
        STEP(H, d, a, b, c, GET(8), 0x8771f681, 11)
        STEP(H, c, d, a, b, GET(11), 0x6d9d6122, 16)
        STEP(H, b, c, d, a, GET(14), 0xfde5380c, 23)
        STEP(H, a, b, c, d, GET(1), 0xa4beea44, 4)
        STEP(H, d, a, b, c, GET(4), 0x4bdecfa9, 11)
        STEP(H, c, d, a, b, GET(7), 0xf6bb4b60, 16)
        STEP(H, b, c, d, a, GET(10), 0xbebfbc70, 23)
        STEP(H, a, b, c, d, GET(13), 0x289b7ec6, 4)
        STEP(H, d, a, b, c, GET(0), 0xeaa127fa, 11)
        STEP(H, c, d, a, b, GET(3), 0xd4ef3085, 16)
        STEP(H, b, c, d, a, GET(6), 0x04881d05, 23)
        STEP(H, a, b, c, d, GET(9), 0xd9d4d039, 4)
        STEP(H, d, a, b, c, GET(12), 0xe6db99e5, 11)
        STEP(H, c, d, a, b, GET(15), 0x1fa27cf8, 16)
        STEP(H, b, c, d, a, GET(2), 0xc4ac5665, 23)
        STEP(I, a, b, c, d, GET(0), 0xf4292244, 6)
        STEP(I, d, a, b, c, GET(7), 0x432aff97, 10)
        STEP(I, c, d, a, b, GET(14), 0xab9423a7, 15)
        STEP(I, b, c, d, a, GET(5), 0xfc93a039, 21)
        STEP(I, a, b, c, d, GET(12), 0x655b59c3, 6)
        STEP(I, d, a, b, c, GET(3), 0x8f0ccc92, 10)
        STEP(I, c, d, a, b, GET(10), 0xffeff47d, 15)
        STEP(I, b, c, d, a, GET(1), 0x85845dd1, 21)
        STEP(I, a, b, c, d, GET(8), 0x6fa87e4f, 6)
        STEP(I, d, a, b, c, GET(15), 0xfe2ce6e0, 10)
        STEP(I, c, d, a, b, GET(6), 0xa3014314, 15)
        STEP(I, b, c, d, a, GET(13), 0x4e0811a1, 21)
        STEP(I, a, b, c, d, GET(4), 0xf7537e82, 6)
        STEP(I, d, a, b, c, GET(11), 0xbd3af235, 10)
        STEP(I, c, d, a, b, GET(2), 0x2ad7d2bb, 15)
        STEP(I, b, c, d, a, GET(9), 0xeb86d391, 21)
        
        a += saved_a;
        b += saved_b;
        c += saved_c;
        d += saved_d;
        
        ptr += 64;
    } while (size -= 64);
    
    ctx->a = a;
    ctx->b = b;
    ctx->c = c;
    ctx->d = d;
    
    return ptr;
}

static void ag_md5_init(ag_md5_ctx *ctx){
    ctx->a = 0x67452301;
    ctx->b = 0xefcdab89;
    ctx->c = 0x98badcfe;
    ctx->d = 0x10325476;
    
    ctx->lo = 0;
    ctx->hi = 0;
}

static void ag_md5_update(ag_md5_ctx *ctx, const void *data, size_t size)
{
    MD5_u32 saved_lo;
    unsigned long used, free;
    
    saved_lo = ctx->lo;
    if ((ctx->lo = (saved_lo + size) & 0x1fffffff) < saved_lo)
        ctx->hi++;
    ctx->hi += size >> 29;
    used = saved_lo & 0x3f;
    
    if (used){
        free = 64 - used;
        if (size < free) {
            memcpy(&ctx->buffer[used], data, size);
            return;
        }
        
        memcpy(&ctx->buffer[used], data, free);
        data = (unsigned char *)data + free;
        size -= free;
        body(ctx, ctx->buffer, 64);
    }
    
    if (size >= 64) {
        data = body(ctx, data, size & ~(unsigned long)0x3f);
        size &= 0x3f;
    }
    
    memcpy(ctx->buffer, data, size);
}

static void ag_md5_final(unsigned char *result, ag_md5_ctx *ctx)
{
    unsigned long used, free;
    used = ctx->lo & 0x3f;
    ctx->buffer[used++] = 0x80;
    free = 64 - used;
    
    if (free < 8) {
        memset(&ctx->buffer[used], 0, free);
        body(ctx, ctx->buffer, 64);
        used = 0;
        free = 64;
    }
    
    memset(&ctx->buffer[used], 0, free - 8);
    
    ctx->lo <<= 3;
    ctx->buffer[56] = ctx->lo;
    ctx->buffer[57] = ctx->lo >> 8;
    ctx->buffer[58] = ctx->lo >> 16;
    ctx->buffer[59] = ctx->lo >> 24;
    ctx->buffer[60] = ctx->hi;
    ctx->buffer[61] = ctx->hi >> 8;
    ctx->buffer[62] = ctx->hi >> 16;
    ctx->buffer[63] = ctx->hi >> 24;
    body(ctx, ctx->buffer, 64);
    result[0] = ctx->a;
    result[1] = ctx->a >> 8;
    result[2] = ctx->a >> 16;
    result[3] = ctx->a >> 24;
    result[4] = ctx->b;
    result[5] = ctx->b >> 8;
    result[6] = ctx->b >> 16;
    result[7] = ctx->b >> 24;
    result[8] = ctx->c;
    result[9] = ctx->c >> 8;
    result[10] = ctx->c >> 16;
    result[11] = ctx->c >> 24;
    result[12] = ctx->d;
    result[13] = ctx->d >> 8;
    result[14] = ctx->d >> 16;
    result[15] = ctx->d >> 24;
    memset(ctx, 0, sizeof(*ctx));
}

/* Return Calculated raw result(always little-endian), the size is always 16 */
void ag_md5bin(const void* dat, size_t len, unsigned char out[16]) 
{
    ag_md5_ctx c;
    ag_md5_init(&c);
    ag_md5_update(&c, dat, len);
    ag_md5_final(out, &c);
}

static char hb2hex(unsigned char hb) {
    hb = hb & 0xF;
    return hb < 10 ? '0' + hb : hb - 10 + 'A';
}

