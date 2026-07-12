/*
 * Compile-only CI shim for coucal's bundled-MD5 backend
 * (-DHTS_INTHASH_USES_MD5). This is NOT a real MD5 implementation.
 *
 * HTTrack builds coucal.c with its own src/md5.h; coucal standalone has none,
 * so this header mirrors that API surface (struct/typedef/prototypes) purely so
 * CI can *compile-check* the HTS_INTHASH_USES_MD5 branch under -Werror and keep
 * it from rotting between httrack submodule bumps. It is deliberately not part
 * of `make dist` and is never linked; functional coverage of the MD5 backend
 * lives in httrack's own CI. Kept API-compatible with httrack/src/md5.h.
 */
#ifndef MD5_H
#define MD5_H

#include <stdint.h>

typedef uint32_t uint32;

struct MD5Context {
  union {
    unsigned char ui8[64];
    uint32 ui32[16];
  } in;
  uint32 buf[4];
  uint32 bits[2];
  int doByteReverse;
};

void MD5Init(struct MD5Context *context, int brokenEndian);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
               unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);

typedef struct MD5Context MD5CTX;

#endif /* !MD5_H */
