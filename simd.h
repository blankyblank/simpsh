#ifndef SIMD_H
#define SIMD_H

#include <emmintrin.h>
#include <stddef.h>
#include <string.h>

/* simd optimized scan to end of word */
static inline size_t
sscnword(const char *buf,size_t len)
{
  __m128i input, ge_lo, le_hi;
  __m128i classmask, wordmask, delimmask, allones;
  __m128i lv_a, hv_z, lv_A, hv_Z, lv_0, hv_9, v_u;
  int mask;
  size_t i;

  lv_a = _mm_set1_epi8('a' - 1);
  hv_z = _mm_set1_epi8('z' + 1);
  lv_A = _mm_set1_epi8('A' - 1);
  hv_Z = _mm_set1_epi8('Z' + 1);
  lv_0 = _mm_set1_epi8('0' - 1);
  hv_9 = _mm_set1_epi8('9' + 1);
  v_u = _mm_set1_epi8('_');
  allones = _mm_set1_epi8(-1);
  i = 0;
  for (; i < len; i += 16) {
    size_t chunk = len - i;
    if (chunk > 16)
      chunk = 16;
    input = _mm_loadu_si128((const __m128i *)(buf + i));

    wordmask = _mm_setzero_si128();
    ge_lo = _mm_cmpgt_epi8(input, lv_a);
    le_hi = _mm_cmplt_epi8(input, hv_z);
    classmask = _mm_and_si128(ge_lo, le_hi);
    wordmask = _mm_or_si128(wordmask, classmask);

    ge_lo = _mm_cmpgt_epi8(input, lv_A);
    le_hi = _mm_cmplt_epi8(input, hv_Z);
    classmask = _mm_and_si128(ge_lo, le_hi);
    wordmask = _mm_or_si128(wordmask, classmask);

    ge_lo = _mm_cmpgt_epi8(input, lv_0);
    le_hi = _mm_cmplt_epi8(input, hv_9);
    classmask = _mm_and_si128(ge_lo, le_hi);
    wordmask = _mm_or_si128(wordmask, classmask);

    classmask = _mm_cmpeq_epi8(input, v_u);
    wordmask = _mm_or_si128(wordmask, classmask);

    delimmask = _mm_andnot_si128(wordmask, allones);
    mask = _mm_movemask_epi8(delimmask);
    mask &= (1 << chunk) - 1;
    if (mask)
      return i + __builtin_ctz(mask);
  }
  return len;
}

/* simd optimized find and skip \t and spaces */
static inline size_t
sskipspace(const char *buf, size_t len)
{
  __m128i input, spacev, tabv, allones;
  __m128i spacer, tabr, spacem, notspacem;
  int mask;
  size_t i;

  spacev = _mm_set1_epi8(' ');
  tabv = _mm_set1_epi8('\t');
  allones = _mm_set1_epi8(-1);

  i = 0;
  for (; i < len; i += 16) {
    size_t chunk = len - i;
    if (chunk > 16)
      chunk = 16;
    input = _mm_loadu_si128((const __m128i *)(buf + i));
    spacer = _mm_cmpeq_epi8(input, spacev);
    tabr = _mm_cmpeq_epi8(input, tabv);
    spacem = _mm_or_si128(spacer, tabr);
    notspacem = _mm_andnot_si128(spacem, allones);
    mask = _mm_movemask_epi8(notspacem);
    mask &= (1 << chunk) - 1;
    if (mask)
      return i + __builtin_ctz(mask);
  }
  return len;
}

/* simd optimized find and skip newlines */
static inline size_t
sskipnl(const char *buf, size_t len)
{
  __m128i input, nlv, allones, nlr, notnlm;
  int mask;
  size_t i;

  allones = _mm_set1_epi8(-1);
  nlv = _mm_set1_epi8('\n');
  mask = 0;
  i = 0;
  for (; i < len; i += 16) {
    size_t chunk = len - i;
    if (chunk > 16)
      chunk = 16;
    input = _mm_loadu_si128((const __m128i *)(buf + i));
    nlr = _mm_cmpeq_epi8(input, nlv);
    notnlm = _mm_andnot_si128(nlr, allones);
    mask = _mm_movemask_epi8(notnlm);
    mask &= (1 << chunk) - 1;
    if (mask)
    return i + __builtin_ctz(mask);
  }
  return len;
}


/* simd optimized scan for delimeters */
static inline size_t
sscndelim(const char *restrict buf, size_t len, const char *restrict delims, int ndelims)
{
  __m128i input, match, delim_vec;
  int mask, d;
  size_t i;

  i = 0;
  for (; i < len; i += 16) {
    size_t chunk = len - i;
    if (chunk > 16)
      chunk = 16;
    input = _mm_loadu_si128((const __m128i *)(buf + i));
    match = _mm_setzero_si128();

    for (d = 0; d < ndelims; d++) {
      delim_vec = _mm_set1_epi8(delims[d]);
      match = _mm_or_si128(match, _mm_cmpeq_epi8(input, delim_vec));
    }

    mask = _mm_movemask_epi8(match);
    mask &= (1 << chunk) - 1;
    if (mask)
      return i + __builtin_ctz(mask);
  }
  return len;
}

/* simd optimized skip delimeters */
static inline size_t
sskipdelims(const char *restrict buf, size_t len,
                 const char *restrict delims, int ndelims)
{
  __m128i input, match, delim_vec;
  int mask, d;
  size_t i;

  i = 0;
  for (; i < len; i += 16) {
    size_t chunk = len - i;
    if (chunk > 16)
      chunk = 16;
    input = _mm_loadu_si128((const __m128i *)(buf + i));
    match = _mm_setzero_si128();

    for (d = 0; d < ndelims; d++) {
      delim_vec = _mm_set1_epi8(delims[d]);
      match = _mm_or_si128(match, _mm_cmpeq_epi8(input, delim_vec));
    }

    mask = _mm_movemask_epi8(match);
    mask &= (1 << chunk) - 1;
    mask = ~mask & ((1 << chunk) - 1);
    if (mask)
      return i + __builtin_ctz(mask);
  }
  return len;
}

static inline size_t
memchr_(const char *buf, size_t len, char c)
{
  __m128i input, target, cmp;
  int mask;
  size_t i;

  target = _mm_set1_epi8(c);
  i = 0;
  for (; i < len; i += 16) {
    size_t chunk = len - i;
    if (chunk > 16)
      chunk = 16;
    input = _mm_loadu_si128((const __m128i *)(buf + i));
    cmp = _mm_cmpeq_epi8(input, target);
    mask = _mm_movemask_epi8(cmp);
    mask &= (1 << chunk) - 1;
    if (mask)
      return i + __builtin_ctz(mask);
  }
  return len;
}

static inline int
smemcmp(const char *restrict a, const char *restrict b, size_t nlen)
{
  static const unsigned short _eq_msk[17] = {
    0,     0x1,   0x3,   0x7,   0xF,    0x1F,   0x3F,   0x7F,  0xFF,
    0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF
  };

  __m128i va = _mm_loadu_si128((const __m128i *)a);
  __m128i vb = _mm_loadu_si128((const __m128i *)b);
  int m = _mm_movemask_epi8(_mm_cmpeq_epi8(va, vb));
  int bitmask = _eq_msk[nlen > 16 ? 16 : nlen];
  if ((m & bitmask) != bitmask)
    return 0;
  return nlen <= 16 || memcmp(a + 16, b + 16, nlen - 16) == 0;
}

#endif /* SIMD_H */

