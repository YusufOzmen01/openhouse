#pragma once
/* The install image's payload is bank A's image packed (tools/lzpack.py), so it fits the "sd" stub's
   40 KB beside the stub. The stub unpacks it straight into flash; tools/lzcheck.c runs this same code on
   the build host against every image.

   Payload: u32 LZ_MAGIC, u32 unpacked size, u32 CRC-32 of the unpacked image (bank_crc32), u32 stream
   length, then the stream (padded to a whole word).
   Stream: sequences of [token][literal length ext][literals][u16 offset][match length ext]. The token's
   high nibble is the literal count, its low nibble the match length - 4; 15 means extension bytes follow
   (each added, until one under 255). The last sequence is literals only: the stream ends after them.
   A match copies length bytes from offset bytes back in the output (offset 1..65535, may overlap). */
#include <stdint.h>

#define LZ_MAGIC 0x5A4C484Fu   /* "OHLZ" */

typedef struct {
  void (*put)(void *ctx, uint8_t b);           /* the next output byte */
  uint8_t (*get)(void *ctx, uint32_t pos);     /* an output byte already put */
  void *ctx;
} lz_sink_t;

static inline uint32_t lz_ext(const uint8_t **s, const uint8_t *end, uint32_t v, int *bad)
{
  if (v != 15) return v;
  uint8_t b;
  do {
    if (*s >= end) { *bad = 1; return 0; }
    b = *(*s)++;
    v += b;
  } while (b == 255);
  return v;
}

/* Unpacks the stream (n bytes) into exactly raw bytes; 0 on a malformed stream or a size mismatch. */
static inline int lz_unpack(const uint8_t *s, uint32_t n, uint32_t raw, const lz_sink_t *k)
{
  const uint8_t *end = s + n;
  uint32_t out = 0;
  int bad = 0;
  while (s < end) {
    const uint8_t t = *s++;
    uint32_t lit = lz_ext(&s, end, t >> 4, &bad);
    if (bad || lit > (uint32_t)(end - s) || lit > raw - out) return 0;
    while (lit--) { k->put(k->ctx, *s++); out++; }
    if (s == end) break;                                   /* the last sequence: literals only */
    if (end - s < 2) return 0;
    const uint32_t off = (uint32_t)s[0] | (uint32_t)s[1] << 8;
    s += 2;
    const uint32_t len = lz_ext(&s, end, t & 15u, &bad) + 4;
    if (bad || off == 0 || off > out || len > raw - out) return 0;
    for (uint32_t i = 0; i < len; i++, out++) k->put(k->ctx, k->get(k->ctx, out - off));
  }
  return out == raw;
}
