#ifndef SIMD_H
#define SIMD_H

#include <emmintrin.h>
#include <stddef.h>

static inline size_t
simd_scan_word(const char *buf,size_t len)
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
  for (; i + 16 <= len; i += 16) {
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
    if (mask)
      return i + __builtin_ctz(mask);
  }
  for (;i < len; i++) {
    char c = buf[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
    (c >= '0' && c <= '9') || c == '_'))
      return i;
  }
  return len;
}

static inline size_t
simd_skip_spaces(const char *buf, size_t len)
{
  __m128i input, spacev, tabv, allones;
  __m128i spacer, tabr, spacem, notspacem;
  int mask;
  size_t i;

  i = 0;
  spacev = _mm_set1_epi8(' ');
  tabv = _mm_set1_epi8('\t');
  allones = _mm_set1_epi8(-1);

  for (;i + 16 <= len; i += 16) {
  input = _mm_loadu_si128((const __m128i *)(buf + i));
  spacer = _mm_cmpeq_epi8(input, spacev);
  tabr = _mm_cmpeq_epi8(input, tabv);
  spacem = _mm_or_si128(spacer, tabr);
  notspacem = _mm_andnot_si128(spacem, allones);
  if ((mask = _mm_movemask_epi8(notspacem)))
    return i + __builtin_ctz(mask);
  }
  for (; i < len; i++)
    if (buf[i] != ' ' && buf[i] != '\t')
      return i;
  return len;
}

static inline size_t
simd_skip_nl(const char *buf, size_t len)
{
  __m128i input, nlv, allones, nlr, notnlm;
  int mask;
  size_t i;

  allones = _mm_set1_epi8(-1);
  nlv = _mm_set1_epi8('\n');
  mask = 0;
  i = 0;
  for (;i + 16 <= len; i += 16) {
    input = _mm_loadu_si128((const __m128i *)(buf + i));
    nlr = _mm_cmpeq_epi8(input, nlv);
    notnlm = _mm_andnot_si128(nlr, allones);
    mask = _mm_movemask_epi8(notnlm);
    if (mask)
    return i + __builtin_ctz(mask);
  }

  for (; i < len; i++)
    if (buf[i] != '\n')
      return i;
  return len;
}


static inline size_t
simd_scan_delim(const char *buf, size_t len, const char *delims, int ndelims)
{
  __m128i input, match, delim_vec;
  int mask, d;
  size_t i;

  i = 0;
  for (; i + 16 <= len; i += 16) {
    input = _mm_loadu_si128((const __m128i *)(buf + i));
    match = _mm_setzero_si128();

    for (d = 0; d < ndelims; d++) {
      delim_vec = _mm_set1_epi8(delims[d]);
      match = _mm_or_si128(match, _mm_cmpeq_epi8(input, delim_vec));
    }

    mask = _mm_movemask_epi8(match);
    if (mask)
      return i + __builtin_ctz(mask);
  }

  for (; i < len; i++)
    for (d = 0; d < ndelims; d++)
      if (buf[i] == delims[d])
        return i;

  return len;
}

static inline size_t
simd_memchr_eq(const char *buf, size_t len, char c)
{
  __m128i input, target, cmp;
  int mask;
  size_t i;

  target = _mm_set1_epi8(c);
  i = 0;
  for (; i + 16 <= len; i += 16) {
    input = _mm_loadu_si128((const __m128i *)(buf + i));
    cmp = _mm_cmpeq_epi8(input, target);
    mask = _mm_movemask_epi8(cmp);
    if (mask)
      return i + __builtin_ctz(mask);
  }
  for (; i < len; i++)
    if (buf[i] == c)
      return i;
  return len;
}
#endif /* SIMD_H */

