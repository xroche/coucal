/* ------------------------------------------------------------ */
/*
Coucal, Cuckoo hashing-based hashtable with stash area.
Copyright (C) 2013-2014 Xavier Roche and other contributors
All rights reserved. BSD 3-Clause (see LICENSE).
*/

/*
 * Differential libFuzzer harness for coucal.
 *
 * The input is decoded as a stream of hashtable operations (write / remove /
 * lookup / inc over a small, heavily colliding key space). Each op is applied
 * to both coucal and a trivial reference model (a flat array indexed by key
 * id); any divergence aborts, so libFuzzer + ASan/UBSan reports it. This is
 * the same differential idea as coucal_test_oracle() in tests.c, but driven by
 * fuzzer-controlled bytes instead of a fixed PRNG stream.
 *
 * Build modes:
 *   libFuzzer  (CI):  clang -fsanitize=fuzzer,address,undefined \
 *                       -DHTS_INTHASH_USES_MURMUR -D_REENTRANT \
 *                       fuzz/coucal_fuzz.c coucal.c -o coucal_fuzz
 *   standalone (any):  cc -DCOUCAL_FUZZ_STANDALONE ... coucal_fuzz.c coucal.c
 *                       -> replays each argv file once; no libFuzzer needed.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "coucal.h"

/* Bounded key space so writes, replaces, removes and lookups collide often. */
#define FZ_KEYS 256u

/* Silence coucal's per-delete stats logging (the harness deletes a table on
   every call); assert failures still abort because the fatal handler is left
   NULL, so genuine invariant breaks are not swallowed. */
static void fz_noop_log(coucal_opaque arg, coucal_loglevel level,
                        const char *format, va_list args) {
  (void) arg;
  (void) level;
  (void) format;
  (void) args;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  coucal h = coucal_new(0);
  intptr_t mval[FZ_KEYS];
  unsigned char mpresent[FZ_KEYS];
  size_t model_count = 0;
  intptr_t counter = 0;
  size_t i;

  coucal_set_assert_handler(h, fz_noop_log, NULL, NULL);
  memset(mpresent, 0, sizeof(mpresent));
  memset(mval, 0, sizeof(mval));

  /* Two bytes per op: [opcode][key id]. */
  for (i = 0; i + 1 < size; i += 2) {
    unsigned op = data[i] & 3u;
    unsigned kid = data[i + 1] % FZ_KEYS;
    char key[16];
    snprintf(key, sizeof(key), "k%u", kid);

    if (op == 0u) {                        /* write / replace */
      intptr_t v = ++counter;              /* non-zero, unique per write */
      int added = coucal_write(h, key, v);
      if (mpresent[kid]) {
        if (added != 0) abort();
      } else {
        if (added == 0) abort();
        mpresent[kid] = 1;
        model_count++;
      }
      mval[kid] = v;
    } else if (op == 1u) {                 /* remove */
      int removed = coucal_remove(h, key);
      if (mpresent[kid]) {
        if (removed == 0) abort();
        mpresent[kid] = 0;
        model_count--;
      } else {
        if (removed != 0) abort();
      }
    } else if (op == 2u) {                 /* lookup */
      intptr_t got;
      if ((coucal_exists(h, key) != 0) != (mpresent[kid] != 0)) abort();
      if (mpresent[kid]) {
        if (!coucal_read(h, key, &got) || got != mval[kid]) abort();
      }
    } else {                               /* inc */
      intptr_t got = coucal_inc(h, key);
      if (mpresent[kid]) {
        mval[kid] += 1;
      } else {
        mpresent[kid] = 1;
        mval[kid] = 1;
        model_count++;
      }
      if (got != mval[kid]) abort();
    }

    if (coucal_nitems(h) != model_count) abort();
  }

  /* Enumeration must reproduce the model set exactly. */
  {
    struct_coucal_enum e = coucal_enum_new(h);
    unsigned char check[FZ_KEYS];
    coucal_item *it;
    size_t seen = 0;
    memset(check, 0, sizeof(check));
    while ((it = coucal_enum_next(&e)) != NULL) {
      unsigned kid = FZ_KEYS;
      if (sscanf((const char *) it->name, "k%u", &kid) != 1 || kid >= FZ_KEYS)
        abort();
      if (!mpresent[kid] || check[kid]) abort();
      check[kid] = 1;
      if (it->value.intg != mval[kid]) abort();
      seen++;
    }
    if (seen != model_count) abort();
  }

  coucal_delete(&h);
  return 0;
}

#ifdef COUCAL_FUZZ_STANDALONE
/* Replay driver so the harness builds and runs without libFuzzer (e.g. gcc +
   ASan). Each argv file is one test case, applied once. */
int main(int argc, char **argv) {
  int a;
  for (a = 1; a < argc; a++) {
    FILE *fp = fopen(argv[a], "rb");
    long n;
    unsigned char *buf;
    size_t got;
    if (fp == NULL) { perror(argv[a]); return 1; }
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 1; }
    n = ftell(fp);
    if (n < 0 || fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return 1; }
    buf = (unsigned char *) malloc((size_t) n + 1u);
    if (buf == NULL) { fclose(fp); return 1; }
    got = fread(buf, 1, (size_t) n, fp);
    fclose(fp);
    (void) LLVMFuzzerTestOneInput(buf, got);
    free(buf);
  }
  return 0;
}
#endif
