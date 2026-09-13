#ifndef SIMDEXT_H
#define SIMDEXT_H
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */
#ifdef __SSE4_1__
  #include <smmintrin.h>
#endif /* __SSE4_1__ */

#include "simd.h"

#ifdef __SSE2__
static inline size_t
scntnl(const char *buf, size_t len)
{
  sint input, nlv, nlr;
  int mask;
  size_t i, n = 0;

  nlv = _mm_set1_epi8('\n');
  for (i = 0; i + 16 <= len; i += 16) {
    input = _mm_loadu_si128((const sint *)(buf + i));
    nlr = _mm_cmpeq_epi8(input, nlv);
    mask = _mm_movemask_epi8(nlr);
    n += __builtin_popcount((unsigned)mask);
  }
  if (i < len) {
    char tmp[16] __attribute__((aligned(16)));
    size_t rem = len - i;
    memcpy(tmp, buf + i, rem);
    input = _mm_load_si128((const sint *)tmp);
    nlr = _mm_cmpeq_epi8(input, nlv);
    mask = _mm_movemask_epi8(nlr) & ((1 << rem) - 1);
    n += __builtin_popcount((unsigned)mask);
  }
  return n;
}

static inline size_t
scntwords(const char *buf, size_t len, int *inwrd)
{
  sint input, spv, tabv, nlv, wsm;
  int mask, non, starts;
  size_t i, n = 0;

  spv = _mm_set1_epi8(' ');
  tabv = _mm_set1_epi8('\t');
  nlv = _mm_set1_epi8('\n');
  for (i = 0; i + 16 <= len; i += 16) {
    input = _mm_loadu_si128((const sint *)(buf + i));
    wsm = _mm_or_si128(_mm_cmpeq_epi8(input, spv),
                       _mm_or_si128(_mm_cmpeq_epi8(input, tabv),
                                    _mm_cmpeq_epi8(input, nlv)));
    mask = _mm_movemask_epi8(wsm);
    non = ~mask & 0xFFFF;
    starts = non & ~(non << 1);
    if (*inwrd)
      starts &= ~1;
    *inwrd = (non >> 15) & 1;
    n += __builtin_popcount((unsigned)starts);
  }
  if (i < len) {
    char tmp[16] __attribute__((aligned(16)));
    size_t rem = len - i;
    memcpy(tmp, buf + i, rem);
    input = _mm_load_si128((const sint *)tmp);
    wsm = _mm_or_si128(_mm_cmpeq_epi8(input, spv),
                       _mm_or_si128(_mm_cmpeq_epi8(input, tabv),
                                    _mm_cmpeq_epi8(input, nlv)));
    mask = _mm_movemask_epi8(wsm) & ((1 << rem) - 1);
    non = ~mask & ((1 << rem) - 1);
    starts = non & ~(non << 1);
    if (*inwrd)
      starts &= ~1;
    *inwrd = (non >> 15) & 1;
    n += __builtin_popcount((unsigned)starts);
  }
  return n;
}

static inline void
trshift(unsigned char *in, unsigned char *out, size_t n,
    unsigned char lo, unsigned char hi, int delta)
{
  sint vlo, vhi, vd;
  size_t i = 0;
  vlo = _mm_set1_epi8((char)(lo - 1));
  vhi = _mm_set1_epi8((char)(hi + 1));
  vd = _mm_set1_epi8((char)delta);
  for (; i + 16 <= n; i += 16) {
    sint x, inr, y;
    x = _mm_loadu_si128((const sint *)(in + i));
    inr = _mm_and_si128(_mm_cmpgt_epi8(x, vlo), _mm_cmplt_epi8(x, vhi));
    y = _mm_add_epi8(x, _mm_and_si128(inr, vd));
    _mm_storeu_si128((sint *)(out + i), y);
  }
  for (; i < n; i++) {
    unsigned char c = in[i];
    if (c >= lo && c <= hi)
      out[i] = (unsigned char)(c + delta);
    else
      out[i] = c;
  }
}

static inline void
trtarget(unsigned char *in, unsigned char *out, size_t n,
    unsigned char lo, unsigned char hi, unsigned char t, int invert)
{
  sint vlo, vhi, vt;
  size_t i = 0;
  vlo = _mm_set1_epi8((char)(lo - 1));
  vhi = _mm_set1_epi8((char)(hi + 1));
  vt = _mm_set1_epi8((char)t);
  for (; i + 16 <= n; i += 16) {
    sint x, inr, y;
    x = _mm_loadu_si128((const sint *)(in + i));
    inr = _mm_and_si128(_mm_cmpgt_epi8(x, vlo), _mm_cmplt_epi8(x, vhi));
#ifdef __SSE4_1__
    if (!invert)
      y = _mm_blendv_epi8(x, vt, inr);
    else
      y = _mm_blendv_epi8(vt, x, inr);
#else
    if (!invert)
      y = _mm_or_si128(_mm_and_si128(inr, vt), _mm_andnot_si128(inr, x));
    else
      y = _mm_or_si128(_mm_and_si128(inr, x), _mm_andnot_si128(inr, vt));
#endif /* __SSE4_1__ */
    _mm_storeu_si128((sint *)(out + i), y);
  }
  for (; i < n; i++) {
    unsigned char c = in[i];
    if ((c >= lo && c <= hi) != (invert))
      out[i] = t;
    else
      out[i] = c;
  }
}

#else
static inline size_t
cntnl(const char *buf, size_t len)
{
  size_t n = 0;
  for (size_t i = 0; i < len; i++)
    if (buf[i] == '\n')
      n++;
  return n;
}

static inline size_t
cntwords(const char *buf, size_t len, int *inwrd)
{
  size_t n = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)buf[i];
    if (c == ' ' || c == '\t' || c == '\n')
      *inwrd = 0;
    else if (!*inwrd) {
      n++;
      *inwrd = 1;
    }
  }
  return n;
}


#define scntnl(buf, len) (cntnl((buf), (len)))
#define scntwords(buf, len, inwrd) (cntwords((buf), (len), (inwrd)))

static inline void
trshift(unsigned char *in, unsigned char *out, size_t n,
    unsigned char lo, unsigned char hi, int delta)
{
  for (size_t i = 0; i < n; i++) {
    unsigned char c = in[i];
    if (c >= lo && c <= hi)
      out[i] = (unsigned char)(c + delta);
    else
      out[i] = c;
  }
}

static inline void
trtarget(unsigned char *in, unsigned char *out, size_t n,
    unsigned char lo, unsigned char hi, unsigned char t, int invert)
{
  for (size_t i = 0; i < n; i++) {
    unsigned char c = in[i];
    if ((c >= lo && c <= hi) != (invert))
      out[i] = t;
    else
      out[i] = c;
  }
}
#endif /* __SSE2__ */

#endif /* SIMDEXT_H */

