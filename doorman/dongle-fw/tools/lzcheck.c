/* Build-host check of a packed payload (tools/lzpack.py) with the stub's own unpacker (src/lz_unpack.h):
   it must give back the bank image byte for byte, and its header must name that image's size and CRC.
   usage: lzcheck PAYLOAD.lz IMAGE.bin */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lz_unpack.h"

static uint8_t *slurp(const char *path, long *n)
{
  FILE *f = fopen(path, "rb");
  if (!f) { perror(path); exit(1); }
  fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *p = malloc((size_t)*n + 1);
  if (fread(p, 1, (size_t)*n, f) != (size_t)*n) { perror(path); exit(1); }
  fclose(f);
  return p;
}

typedef struct { uint8_t *buf; uint32_t n, cap; } out_t;
static void put(void *c, uint8_t b) { out_t *o = c; if (o->n < o->cap) o->buf[o->n] = b; o->n++; }
static uint8_t get(void *c, uint32_t pos) { return ((out_t *)c)->buf[pos]; }

static uint32_t crc32(const uint8_t *p, uint32_t n)   /* banks.h's bank_crc32 */
{
  uint32_t c = 0xFFFFFFFFu;
  while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
  return ~c;
}

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

int main(int argc, char **argv)
{
  if (argc != 3) { fprintf(stderr, "usage: lzcheck PAYLOAD.lz IMAGE.bin\n"); return 2; }
  long pn, in;
  uint8_t *p = slurp(argv[1], &pn), *img = slurp(argv[2], &in);
  if (pn < 16 || rd32(p) != LZ_MAGIC) { fprintf(stderr, "lzcheck: %s: not a packed payload\n", argv[1]); return 1; }
  const uint32_t raw = rd32(p + 4), crc = rd32(p + 8), len = rd32(p + 12);
  if (len > (uint32_t)pn - 16) { fprintf(stderr, "lzcheck: %s: stream longer than the file\n", argv[1]); return 1; }
  if (raw != (uint32_t)in || crc != crc32(img, (uint32_t)in)) { fprintf(stderr, "lzcheck: %s: header doesn't match %s\n", argv[1], argv[2]); return 1; }
  out_t o = { malloc(raw + 1), 0, raw };
  const lz_sink_t k = { put, get, &o };
  const int ok = lz_unpack(p + 16, len, raw, &k) && o.n == raw && memcmp(o.buf, img, raw) == 0;
  if (!ok) { fprintf(stderr, "lzcheck: %s doesn't unpack to %s\n", argv[1], argv[2]); return 1; }
  printf("lzcheck: %s: %u -> %ld bytes (%.0f %%), unpacks to the image\n", argv[1], raw, pn, 100.0 * pn / raw);
  return 0;
}
