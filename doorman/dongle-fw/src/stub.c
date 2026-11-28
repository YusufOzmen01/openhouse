/* Boot stub (banks.h). Installed by the bootloader at the board's app start, together with a copy of
   the firmware built for bank A (payload). It boots the newest valid bank. It installs the payload
   first when no bank is valid, or when the newest bank's chain didn't start from this payload (the
   bootloader just installed a new one): into the other bank, so the old one stays as a fallback until
   the new one is written. No interrupts, no peripherals besides NVMC. */
#include "nrf.h"
#include "banks.h"
#include "lz_unpack.h"

extern const uint8_t payload_start[], payload_end[];

static void nvmc_wait(void) { while (!NRF_NVMC->READY) {} }

static void erase(uint32_t addr, uint32_t len)
{
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een; nvmc_wait();
  for (uint32_t a = addr; a < addr + len; a += 0x1000) { NRF_NVMC->ERASEPAGE = a; nvmc_wait(); }
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren; nvmc_wait();
}

static void write(uint32_t addr, const uint8_t *src, uint32_t len)
{
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen; nvmc_wait();
  for (uint32_t i = 0; i < len; i += 4) {
    uint32_t w = (uint32_t)src[i] | (uint32_t)src[i + 1] << 8 | (uint32_t)src[i + 2] << 16 | (uint32_t)src[i + 3] << 24;
    *(volatile uint32_t *)(addr + i) = w; nvmc_wait();
  }
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren; nvmc_wait();
}

/* The payload unpacks straight into the bank (lz_unpack.h): whole words are written as they fill, and a
   match reads back what's already there, the word still filling from `acc`. */
typedef struct { uint32_t base, n, acc; } flash_out_t;
static void flash_put(void *c, uint8_t b)
{
  flash_out_t *o = c;
  o->acc |= (uint32_t)b << (8 * (o->n & 3));
  if ((++o->n & 3) == 0) { *(volatile uint32_t *)(o->base + o->n - 4) = o->acc; nvmc_wait(); o->acc = 0; }
}
static uint8_t flash_get(void *c, uint32_t pos)
{
  const flash_out_t *o = c;
  if (pos >= (o->n & ~3u)) return (uint8_t)(o->acc >> (8 * (pos & 3)));
  return *(const volatile uint8_t *)(o->base + pos);
}

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* Bank A's image only runs from bank A: the payload goes there. If bank A is the one to keep, the
   payload can't be installed without losing it, so then bank A is overwritten anyway (the payload is
   what the bootloader was just asked to install). The header goes on only once the unpacked image's
   CRC matches the one the payload names. */
static int install_payload(uint32_t seq, uint32_t origin)
{
  const uint32_t have = (uint32_t)(payload_end - payload_start);
  if (have < 16 || rd32(payload_start) != LZ_MAGIC) return 0;
  const uint32_t size = rd32(payload_start + 4), crc = rd32(payload_start + 8), len = rd32(payload_start + 12);
  if (!size || size > BANK_IMAGE_MAX || (size & 3) || len > have - 16) return 0;
  erase(BANK_A, BANK_IMAGE_OFS + ((size + 0xFFFu) & ~0xFFFu));
  flash_out_t o = { BANK_A + BANK_IMAGE_OFS, 0, 0 };
  const lz_sink_t k = { flash_put, flash_get, &o };
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen; nvmc_wait();
  const int unpacked = lz_unpack(payload_start + 16, len, size, &k);
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren; nvmc_wait();
  if (!unpacked || bank_crc32((const uint8_t *)(BANK_A + BANK_IMAGE_OFS), size) != crc) return 0;
  const bank_header_t h = { BANK_MAGIC, seq, size, crc, origin };
  write(BANK_A, (const uint8_t *)&h, sizeof h);
  return bank_valid(BANK_A);
}

static __attribute__((noreturn)) void jump(uint32_t base)
{
  const uint32_t *vec = (const uint32_t *)(base + BANK_IMAGE_OFS);
  SCB->VTOR = (uint32_t)vec;
  __DSB(); __ISB();
  __set_MSP(vec[0]);
  ((void (*)(void))vec[1])();
  for (;;) {}
}

__attribute__((noreturn)) void stub_main(void)
{
  const uint32_t origin = bank_crc32(payload_start, (uint32_t)(payload_end - payload_start));
  const int a = bank_valid(BANK_A), b = bank_valid(BANK_B);
  uint32_t best = 0;
  if (a) best = BANK_A;
  if (b && (!a || bank_header(BANK_B)->seq > bank_header(BANK_A)->seq)) best = BANK_B;
  if (!best || bank_header(best)->origin != origin) {
    const uint32_t seq = best ? bank_header(best)->seq + 1 : 1;
    if (install_payload(seq, origin)) best = BANK_A;
    else if (best && !bank_valid(best)) best = 0;      /* bank A was the newest and is gone now */
  }
  if (best) jump(best);
  for (;;) { __WFE(); }                                /* nothing to run: the bootloader is the way in */
}

/* The vector table: just the stack and the entry point (no interrupts are enabled here). */
extern uint32_t __stack_top;
__attribute__((section(".isr_vector"), used)) const void *const stub_vectors[2] = { &__stack_top, (void *)stub_main };

/* The payload: bank A's image, packed (tools/lzpack.py), linked in at build time. */
__asm__(".section .payload,\"a\"\n .balign 4\n .global payload_start\npayload_start:\n"
        " .incbin \"" PAYLOAD_BIN "\"\n .balign 4\n .global payload_end\npayload_end:\n .text\n");
