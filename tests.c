/* ------------------------------------------------------------ */
/*
Coucal, Cuckoo hashing-based hashtable with stash area.
Copyright (C) 2013-2014 Xavier Roche and other contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "coucal.h"

/* checks below read coucal's own log output, which the build may compile out */
#define STATS_LOGGED (COUCAL_LOG_LEVEL >= COUCAL_LOG_INFO)
#define KEYS_PRINTED (COUCAL_LOG_LEVEL >= COUCAL_LOG_TRACE)

static size_t fsize(const char *s) {
  struct stat st;

  if (stat(s, &st) == 0 && S_ISREG(st.st_mode)) {
    return st.st_size;
  } else {
    return (size_t) -1;
  }
}

static int coucal_test(const char *snum) {
  unsigned long count = 0;
  const char *const names[] = {
    "", "add", "delete", "dry-add", "dry-del",
    "test-exists", "test-not-exist"
  };
  const struct {
    enum {
      DO_END,
      DO_ADD,
      DO_DEL,
      DO_DRY_ADD,
      DO_DRY_DEL,
      TEST_ADD,
      TEST_DEL
    } type;
    size_t modulus;
    size_t offset;
  } bench[] = {
    { DO_ADD, 4, 0 },     /* add 4/0 */
    { TEST_ADD, 4, 0 },   /* check 4/0 */
    { TEST_DEL, 4, 1 },   /* check 4/1 */
    { TEST_DEL, 4, 2 },   /* check 4/2 */
    { TEST_DEL, 4, 3 },   /* check 4/3 */
    { DO_DRY_DEL, 4, 1 }, /* del 4/1 */
    { DO_DRY_DEL, 4, 2 }, /* del 4/2 */
    { DO_DRY_DEL, 4, 3 }, /* del 4/3 */
    { DO_ADD, 4, 1 },     /* add 4/1 */
    { DO_DRY_ADD, 4, 1 }, /* add 4/1 */
    { TEST_ADD, 4, 0 },   /* check 4/0 */
    { TEST_ADD, 4, 1 },   /* check 4/1 */
    { TEST_DEL, 4, 2 },   /* check 4/2 */
    { TEST_DEL, 4, 3 },   /* check 4/3 */
    { DO_ADD, 4, 2 },     /* add 4/2 */
    { DO_DRY_DEL, 4, 3 }, /* del 4/3 */
    { DO_ADD, 4, 3 },     /* add 4/3 */
    { DO_DEL, 4, 3 },     /* del 4/3 */
    { TEST_ADD, 4, 0 },   /* check 4/0 */
    { TEST_ADD, 4, 1 },   /* check 4/1 */
    { TEST_ADD, 4, 2 },   /* check 4/2 */
    { TEST_DEL, 4, 3 },   /* check 4/3 */
    { DO_DEL, 4, 0 },     /* del 4/0 */
    { DO_DEL, 4, 1 },     /* del 4/1 */
    { DO_DEL, 4, 2 },     /* del 4/2 */
    /* empty here */
    { TEST_DEL, 1, 0 },   /* check */
    { DO_ADD, 4, 0 },     /* add 4/0 */
    { DO_ADD, 4, 1 },     /* add 4/1 */
    { DO_ADD, 4, 2 },     /* add 4/2 */
    { DO_DEL, 42, 0 },    /* add 42/0 */
    { TEST_DEL, 42, 0 },  /* check 42/0 */
    { TEST_ADD, 42, 2 },  /* check 42/2 */
    { DO_END, 0, 0 }
  };
  char *buff = NULL;
  const char **strings = NULL;

  /* produce random patterns, or read from a file */
  if (sscanf(snum, "%lu", &count) != 1) {
    const size_t size = fsize(snum);
    FILE *fp = fopen(snum, "rb");
    if (fp != NULL) {
      buff = malloc(size);
      if (buff != NULL && fread(buff, 1, size, fp) == size) {
        size_t capa = 0;
        size_t i, last;
        for(i = 0, last = 0, count = 0 ; i < size ; i++) {
          if (buff[i] == 10 || buff[i] == 0) {
            buff[i] = '\0';
            if (capa == count) {
              if (capa == 0) {
                capa = 16;
              } else {
                capa <<= 1;
              }
              strings = (const char **) realloc((void*) strings, capa*sizeof(char*));
            }
            strings[count++] = &buff[last];
            last = i + 1;
          }
        }
      }
      fclose(fp);
    }
  }

  /* successfully read */
  if (count > 0) {
    coucal hashtable = coucal_new(0);
    size_t loop;
    for(loop = 0 ; bench[loop].type != DO_END ; loop++) {
      size_t i;
      for(i = bench[loop].offset ; i < (size_t) count
          ; i += bench[loop].modulus) {
        int result = 0;
        char buffer[256];
        const char *name;
        /* unsigned: this LCG overflows a 32-bit long on ILP32 */
        const long expected =
            (long) ((unsigned long) i * 1664525UL + 1013904223UL);
        if (strings == NULL) {
          snprintf(buffer, sizeof(buffer),
            "http://www.example.com/website/sample/for/hashtable/"
            "%ld/index.html?foo=%ld&bar",
            (long) i, (long) (expected));
          name = buffer;
        } else {
          name = strings[i];
        }
        if (bench[loop].type == DO_ADD
            || bench[loop].type == DO_DRY_ADD) {
          size_t k;
          result = coucal_write(hashtable, name, (uintptr_t) expected);
          for(k = 0 ; k < /* stash_size*2 */ 32 ; k++) {
            (void) coucal_write(hashtable, name, (uintptr_t) expected);
          }
          /* revert logic */
          if (bench[loop].type == DO_DRY_ADD) {
            result = result ? 0 : 1;
          }
        }
        else if (bench[loop].type == DO_DEL
            || bench[loop].type == DO_DRY_DEL) {
          size_t k;
          result = coucal_remove(hashtable, name);
          for(k = 0 ; k < /* stash_size*2 */ 32 ; k++) {
            (void) coucal_remove(hashtable, name);
          }
          /* revert logic */
          if (bench[loop].type == DO_DRY_DEL) {
            result = result ? 0 : 1;
          }
        }
        else if (bench[loop].type == TEST_ADD
            || bench[loop].type == TEST_DEL) {
          intptr_t value = -1;
          result = coucal_readptr(hashtable, name, &value);
          if (bench[loop].type == TEST_ADD && result
              && value != expected) {
            fprintf(stderr, "value failed for %s (expected %ld, got %ld)\n",
                    name, (long) expected, (long) value);
            return EXIT_FAILURE;
          }
          /* revert logic */
          if (bench[loop].type == TEST_DEL) {
            result = result ? 0 : 1;
          }
        }
        if (!result) {
          fprintf(stderr, "failed %s{%d/+%d} test on loop %ld"
                  " at offset %ld for %s\n",
                  names[bench[loop].type],
                  (int) bench[loop].modulus,
                  (int) bench[loop].offset,
                  (long) loop, (long) i, name);
          return EXIT_FAILURE;
        }
      }
    }
    coucal_delete(&hashtable);
    fprintf(stderr, "all hashtable tests were successful!\n");
    return EXIT_SUCCESS;
  } else {
    fprintf(stderr, "Malformed number\n");
    return EXIT_FAILURE;
  }
}

/* Regression for the MurmurHash3 tail UB: a key byte landing in a "<< 24" slot
   with the high bit set used to overflow signed int (UBSan). Lengths 4/8/12 put
   that byte last; hash and read each back so a sanitized run guards the fix. */
static int coucal_test_high_bytes(void) {
  static const char *const keys[] = {
    "abc\xC3",        /* len 4  -> tail[3]  << 24 */
    "abcdefg\xC3",    /* len 8  -> tail[7]  << 24 */
    "abcdefghijk\xC3" /* len 12 -> tail[11] << 24 */
  };
  coucal hashtable = coucal_new(0);
  size_t i;
  for(i = 0 ; i < sizeof(keys)/sizeof(keys[0]) ; i++) {
    coucal_write(hashtable, keys[i], (uintptr_t) (i + 1));
  }
  for(i = 0 ; i < sizeof(keys)/sizeof(keys[0]) ; i++) {
    intptr_t value = -1;
    if (!coucal_readptr(hashtable, keys[i], &value)
        || value != (intptr_t) (i + 1)) {
      fprintf(stderr, "high-byte key %lu failed\n", (unsigned long) i);
      coucal_delete(&hashtable);
      return EXIT_FAILURE;
    }
  }
  coucal_delete(&hashtable);
  return EXIT_SUCCESS;
}

/* Fail the current test function, printing the offending expression. */
#define CHECK(COND) do {                                                \
    if (!(COND)) {                                                      \
      fprintf(stderr, "CHECK failed: %s (%s:%d)\n",                     \
              #COND, __FILE__, __LINE__);                               \
      return EXIT_FAILURE;                                              \
    }                                                                   \
  } while (0)

/* Coverage of the public read/write/enumerate API surface that the big
   stress benchmark above does not touch. Assertions follow coucal.h's
   documented contract, except coucal_inc/coucal_dec, whose header text is a
   stale copy of coucal_write's ("non-zero if added"): the implementation
   returns the new counter value, so we assert the unambiguous stored value
   rather than the return. */
static int coucal_test_api(void) {
  coucal h = coucal_new(0);
  intptr_t iv;
  coucal_value val;
  coucal_value out;
  void *pv;
  int i;

  CHECK(coucal_created(h));
  CHECK(coucal_hash_size() == COUCAL_HASH_SIZE);
  CHECK(coucal_nitems(h) == 0);

  /* write / replace / read / exists / get_intptr */
  CHECK(coucal_write(h, "alpha", 111) != 0);   /* added   */
  CHECK(coucal_write(h, "alpha", 222) == 0);   /* replaced */
  CHECK(coucal_exists(h, "alpha"));
  CHECK(!coucal_exists(h, "absent"));
  CHECK(coucal_read(h, "alpha", &iv) && iv == 222);
  CHECK(coucal_get_intptr(h, "alpha") == 222);
  CHECK(coucal_get_intptr(h, "absent") == 0);
  CHECK(coucal_nitems(h) == 1);

  /* value-union read/write */
  val.intg = 4242;
  CHECK(coucal_write_value(h, "v", val) != 0);
  CHECK(coucal_read_value(h, "v", &out) && out.intg == 4242);

  /* fetch a mutable value pointer and change it in place */
  {
    coucal_value *p = coucal_fetch_value(h, "v");
    CHECK(p != NULL && p->intg == 4242);
    p->intg = 99;
  }
  CHECK(coucal_get_intptr(h, "v") == 99);
  CHECK(coucal_fetch_value(h, "absent") == NULL);

  /* pointer variants */
  CHECK(coucal_write_pvoid(h, "ptr", (void *) "hello") != 0);
  CHECK(coucal_get_pvoid(h, "ptr") != NULL);
  CHECK(strcmp((const char *) coucal_get_pvoid(h, "ptr"), "hello") == 0);
  CHECK(coucal_read_pvoid(h, "ptr", &pv) && pv != NULL);
  CHECK(coucal_get_pvoid(h, "absent") == NULL);
  coucal_add_pvoid(h, "ptr2", (void *) "world");   /* alias to write_pvoid */
  CHECK(coucal_get_pvoid(h, "ptr2") != NULL);

  /* precomputed-hash fetch must agree with the by-name fetch */
  {
    coucal_hashkeys hk = coucal_calc_hashes(h, "alpha");
    coucal_value *p = coucal_fetch_value_hashes(h, "alpha", &hk);
    CHECK(p != NULL && p->intg == 222);
  }

  /* inc/dec: assert the resulting stored value (see note above) */
  CHECK(!coucal_exists(h, "cnt"));
  (void) coucal_inc(h, "cnt");
  CHECK(coucal_get_intptr(h, "cnt") == 1);   /* created at 1 */
  (void) coucal_inc(h, "cnt");
  CHECK(coucal_get_intptr(h, "cnt") == 2);
  (void) coucal_dec(h, "cnt");
  CHECK(coucal_get_intptr(h, "cnt") == 1);

  /* remove semantics */
  CHECK(coucal_remove(h, "alpha") != 0);
  CHECK(!coucal_exists(h, "alpha"));
  CHECK(coucal_remove(h, "alpha") == 0);     /* already gone */

  /* housekeeping accessors */
  CHECK(coucal_memory_size(h) > 0);
  CHECK(coucal_get_name(h) == NULL);
  CHECK(coucal_get_name(NULL) == NULL); /* the assertion path passes NULL */
  coucal_set_name(h, "mytable");
  CHECK(coucal_get_name(h) != NULL
        && strcmp(coucal_get_name(h), "mytable") == 0);

  coucal_delete(&h);
  CHECK(h == NULL);   /* coucal_delete nulls the caller's pointer */

  /* enumeration must visit every entry exactly once, with correct values */
  h = coucal_new(0);
  for (i = 0; i < 10; i++) {
    char b[16];
    snprintf(b, sizeof(b), "e%d", i);
    coucal_write(h, b, (intptr_t) (i + 1000));
  }
  {
    struct_coucal_enum e = coucal_enum_new(h);
    coucal_item *it;
    int seen[10];
    int count = 0;
    memset(seen, 0, sizeof(seen));
    while ((it = coucal_enum_next(&e)) != NULL) {
      int id = -1;
      CHECK(sscanf((const char *) it->name, "e%d", &id) == 1
            && id >= 0 && id < 10);
      CHECK(it->value.intg == (intptr_t) (id + 1000));
      CHECK(!seen[id]);
      seen[id] = 1;
      count++;
    }
    CHECK(count == 10);
    for (i = 0; i < 10; i++) {
      CHECK(seen[i]);
    }
  }
  coucal_delete(&h);
  return EXIT_SUCCESS;
}

/* pool offsets below don't depend on hash backend or key width */
#define ALIAS_DONOR_LEN 600
#define ALIAS_FILLER_LEN 99

/* look up the pooled key by content: enumeration order is hash-dependent */
static const char *coucal_stored_key(coucal hashtable, const char *name) {
  struct_coucal_enum e = coucal_enum_new(hashtable);
  const coucal_item *item;

  while ((item = coucal_enum_next(&e)) != NULL) {
    if (strcmp((const char *) item->name, name) == 0) {
      return (const char *) item->name;
    }
  }
  return NULL;
}

/* builds a pool with holes so the next growth compacts rather than reallocs */
static coucal coucal_build_holed_pool(const char *donor) {
  coucal h = coucal_new(0);
  char filler[ALIAS_FILLER_LEN + 1];
  int i;

  memset(filler, 'f', sizeof(filler) - 1);
  filler[sizeof(filler) - 1] = '\0';

  filler[0] = '0';
  coucal_write(h, filler, 0);
  coucal_write(h, donor, 1);
  for (i = 1; i < 4; i++) {
    filler[0] = (char) ('0' + i);
    coucal_write(h, filler, i);
  }
  for (i = 0; i < 4; i++) {
    filler[0] = (char) ('0' + i);
    coucal_remove(h, filler);
  }
  return h;
}

/* forces pool growth via `aliased`; the gap to `donor` shows which path ran */
static int coucal_check_aliased_write(coucal h, const char *donor,
                                      const char *aliased) {
  char expected[ALIAS_DONOR_LEN + 1];
  const size_t donor_len = strlen(donor) + 1;
  const char *stored_donor;
  const char *stored_aliased;
  size_t before;

  CHECK(strlen(aliased) < sizeof(expected));
  memcpy(expected, aliased, strlen(aliased) + 1);

  before = coucal_memory_size(h);
  CHECK(coucal_write(h, aliased, 42) != 0); /* added, so the dup path ran */
  CHECK(coucal_memory_size(h) > before);    /* and it did grow the pool */

  CHECK(coucal_exists(h, donor));
  CHECK(coucal_exists(h, expected));
  stored_donor = coucal_stored_key(h, donor);
  stored_aliased = coucal_stored_key(h, expected);
  CHECK(stored_donor != NULL && stored_aliased != NULL);
  CHECK((size_t) (stored_aliased - stored_donor) == donor_len);
  return EXIT_SUCCESS;
}

/* a pool-aliased key must survive the growth its own insertion triggers */
static int coucal_test_pool_alias(void) {
  char donor[ALIAS_DONOR_LEN + 1];
  const char *stored;
  coucal h;

  memset(donor, 'd', ALIAS_DONOR_LEN);
  donor[ALIAS_DONOR_LEN] = '\0';

  /* realloc() growth: the pool has no holes, so compaction cannot be chosen */
  h = coucal_new(0);
  CHECK(coucal_write(h, donor, 1) != 0);
  stored = coucal_stored_key(h, donor);
  CHECK(stored != NULL);
  CHECK(coucal_check_aliased_write(h, donor, stored + 1) == EXIT_SUCCESS);
  coucal_delete(&h);

  /* compaction growth, aliasing a live key: the old pool is freed outright */
  h = coucal_build_holed_pool(donor);
  stored = coucal_stored_key(h, donor);
  CHECK(stored != NULL);
  CHECK(coucal_check_aliased_write(h, donor, stored + 1) == EXIT_SUCCESS);
  coucal_delete(&h);

  /* compaction growth, aliasing a dead filler's tail: nothing tracks it */
  h = coucal_build_holed_pool(donor);
  stored = coucal_stored_key(h, donor);
  CHECK(stored != NULL);
  CHECK(coucal_check_aliased_write(h, donor, stored + ALIAS_DONOR_LEN + 4) ==
        EXIT_SUCCESS);
  coucal_delete(&h);

  return EXIT_SUCCESS;
}

/* An out-of-range initial size is rejected, not shifted by the size_t width. */
static int coucal_test_new_size(void) {
  coucal h;

  CHECK(coucal_new((size_t) -1) == NULL);
  h = coucal_new(1024);
  CHECK(coucal_write(h, "key", 1) != 0);
  coucal_delete(&h);
  return EXIT_SUCCESS;
}

/* The value free-handler must fire exactly once per value that leaves the
   table -- on replace (old value), on remove, and on delete (survivors) --
   and never otherwise. A miscount, or a run flagged by the leak sanitizer,
   fails the test. */
static unsigned g_freed;
static void test_free_handler(coucal_opaque arg, coucal_value value) {
  (void) arg;
  g_freed++;
  free(value.ptr);
}

static int coucal_test_value_handler(void) {
  coucal h = coucal_new(0);
  int i;

  g_freed = 0;
  coucal_value_set_value_handler(h, test_free_handler, NULL);

  /* five heap-owned values; nothing freed yet */
  for (i = 0; i < 5; i++) {
    char b[16];
    int *p = (int *) malloc(sizeof(*p));
    CHECK(p != NULL);
    *p = i;
    snprintf(b, sizeof(b), "h%d", i);
    coucal_write_pvoid(h, b, p);
  }
  CHECK(g_freed == 0);

  /* replacing h0 frees its previous value */
  {
    int *p = (int *) malloc(sizeof(*p));
    CHECK(p != NULL);
    *p = 42;
    coucal_write_pvoid(h, "h0", p);
  }
  CHECK(g_freed == 1);

  /* removing h1 frees its value */
  CHECK(coucal_remove(h, "h1") != 0);
  CHECK(g_freed == 2);

  /* deleting frees the four survivors (h0's replacement, h2, h3, h4) */
  coucal_delete(&h);
  CHECK(g_freed == 6);
  return EXIT_SUCCESS;
}

/* key free-handler: once per dup'd key, on remove/delete (survivors only) */
static unsigned g_key_dups, g_key_frees;
static size_t g_stash_size;
static coucal_key test_key_dup(coucal_opaque arg, coucal_key_const name) {
  (void) arg;
  g_key_dups++;
  return strdup((const char *) name);
}
static void test_key_free(coucal_opaque arg, coucal_key name) {
  (void) arg;
  g_key_frees++;
  free(name);
}

/* three keys per group share two hash slots, so one lands in the stash */
static coucal_hashkeys test_key_hash(coucal_opaque arg, coucal_key_const name) {
  const int group = atoi((const char *) name + 1) / 3;
  coucal_hashkeys k;
  (void) arg;
  k.hash1 = (coucal_hashkey) (group * 3 + 1);
  k.hash2 = (coucal_hashkey) (group * 3 + 2);
  return k;
}

/* coucal_delete() logs the summary: only public window onto stash.size */
static void test_key_log(coucal_opaque arg, coucal_loglevel level,
                         const char *format, va_list args) {
  char line[1024];
  const char *p;
  (void) arg;
  (void) level;
  vsnprintf(line, sizeof(line), format, args);
  p = strstr(line, " stash-size=");
  if (p != NULL) {
    g_stash_size = (size_t) atol(p + sizeof(" stash-size=") - 1);
  }
}

static int coucal_test_key_handler(void) {
#define KEY_HANDLER_KEYS 4096
#define KEY_HANDLER_STASHED_KEYS 12
  coucal h = coucal_new(0);
  char b[16];
  int i;

  g_key_dups = 0;
  g_key_frees = 0;
  coucal_value_set_key_handler(h, test_key_dup, test_key_free, NULL, NULL,
                               NULL);

  /* enough keys to force rehashes */
  for (i = 0; i < KEY_HANDLER_KEYS; i++) {
    snprintf(b, sizeof(b), "k%d", i);
    CHECK(coucal_write(h, b, i) != 0);
  }
  CHECK(g_key_dups == KEY_HANDLER_KEYS);
  CHECK(g_key_frees == 0);

  /* a write over an existing key keeps the stored key */
  CHECK(coucal_write(h, "k0", -1) == 0);
  CHECK(g_key_dups == KEY_HANDLER_KEYS);
  CHECK(g_key_frees == 0);

  CHECK(coucal_remove(h, "k1") != 0);
  CHECK(g_key_frees == 1);

  coucal_delete(&h);
  CHECK(g_key_frees == g_key_dups);

  /* same contract for stashed keys, which no backend populates reliably */
  g_key_dups = 0;
  g_key_frees = 0;
  g_stash_size = 0;
  h = coucal_new(0);
  coucal_value_set_key_handler(h, test_key_dup, test_key_free, test_key_hash,
                               NULL, NULL);
  coucal_set_assert_handler(h, test_key_log, NULL, NULL);
  for (i = 0; i < KEY_HANDLER_STASHED_KEYS; i++) {
    snprintf(b, sizeof(b), "k%d", i);
    CHECK(coucal_write(h, b, i) != 0);
  }
  coucal_delete(&h);
  CHECK(!STATS_LOGGED || g_stash_size != 0);
  CHECK(g_key_frees == g_key_dups);
  return EXIT_SUCCESS;
#undef KEY_HANDLER_STASHED_KEYS
#undef KEY_HANDLER_KEYS
}

static unsigned g_printed;
static const char *test_print_key(coucal_opaque arg, coucal_key_const name) {
  (void) arg;
  g_printed++;
  return (const char *) name;
}
static const char *test_print_value(coucal_opaque arg,
                                    coucal_value_const value) {
  (void) arg;
  (void) value;
  return "?";
}

static char g_logged[1024];
static int g_max_level;
static void test_log_handler(coucal_opaque arg, coucal_loglevel level,
                             const char *format, va_list args) {
  (void) arg;
  if ((int) level > g_max_level) {
    g_max_level = (int) level;
  }
  vsnprintf(g_logged, sizeof(g_logged), format, args);
}

static int coucal_test_hardening(void) {
  coucal h = coucal_new(0);
  const char *moved;
  unsigned long cuckoo_moved = 0;
  int i;

  /* the key-printing call sites all sit at trace level */
  g_printed = 0;
  g_max_level = -1;
  coucal_set_print_handler(h, test_print_key, test_print_value, NULL);
  coucal_set_assert_handler(h, test_log_handler, NULL, NULL);
  for (i = 0; i < 5000; i++) {
    char b[24];
    snprintf(b, sizeof(b), "hard_%d", i);
    coucal_write(h, b, (intptr_t) (i + 1));
  }
  CHECK(KEYS_PRINTED ? g_printed != 0 : g_printed == 0);
  /* inserting logs at debug and trace only, and at exactly the selected level
     when one of them is compiled in (enum n is threshold n+1) */
  CHECK(g_max_level ==
        (COUCAL_LOG_LEVEL >= COUCAL_LOG_DEBUG ? COUCAL_LOG_LEVEL - 1 : -1));

  CHECK(coucal_read(h, "hard_0", NULL) != 0);
  CHECK(coucal_readptr(h, "hard_0", NULL) != 0);
  CHECK(coucal_readptr(h, "absent", NULL) == 0);
  coucal_write(h, "zero", 0);
  CHECK(coucal_readptr(h, "zero", NULL) == 0);

  /* the empty key is the one shared, read-only entry of the string pool */
  CHECK(coucal_write(h, "", 7) != 0);
  CHECK(coucal_get_intptr(h, "") == 7);
  CHECK(coucal_remove(h, "") != 0);
  g_logged[0] = '\0';
  g_max_level = -1;
  coucal_delete(&h);

  if (!STATS_LOGGED) {
    CHECK(g_logged[0] == '\0');
    CHECK(g_max_level == -1);
    return EXIT_SUCCESS;
  }
  CHECK(g_max_level == coucal_log_info); /* destruction logs the summary only */

  /* a non-zero move count proves the cuckoo path above was not vacuous */
  moved = strstr(g_logged, " moved=");
  CHECK(moved != NULL);
  CHECK(sscanf(moved, " moved=%lu", &cuckoo_moved) == 1);
  CHECK(cuckoo_moved != 0);

  /* an empty table must not report a 0/0 average */
  h = coucal_new(0);
  coucal_set_assert_handler(h, test_log_handler, NULL, NULL);
  g_logged[0] = '\0';
  coucal_delete(&h);
  CHECK(strstr(g_logged, "avg-moved=0 ") != NULL);
  CHECK(strstr(g_logged, "nan") == NULL);
  return EXIT_SUCCESS;
}

/* hash values are contractual: pin them, not just "the table still works" */
static const size_t kat_lengths[] = {0, 1, 4, 15, 16, 17, 31, 32, 33, 64};

#if (defined(HTS_INTHASH_USES_MD5) || defined(HTS_INTHASH_USES_OPENSSL_MD5))
/* bundled and OpenSSL both hash with MD5, hence the shared vectors */
#if (COUCAL_HASH_SIZE == 32)
#define KAT_VECTORS
static const uint64_t kat_vectors[][2] = {
    {0x41859d3dUL, 0x7af0f863UL}, {0x5730c9b4UL, 0x669bdf66UL},
    {0x9bce5ae8UL, 0xd6f80007UL}, {0xdb0207ecUL, 0x6596ab43UL},
    {0xf574fd85UL, 0x4adc0cbfUL}, {0xddf3d440UL, 0x75a80183UL},
    {0xeb956e99UL, 0xc022457eUL}, {0xf4c6b6c9UL, 0xcf63c053UL},
    {0xd6f7e72bUL, 0xddbe6383UL}, {0xf0dd1d57UL, 0x546bee2aUL}};
#else
#define KAT_VECTORS
static const uint64_t kat_vectors[][2] = {
    {0x04b2008fd98c1dd4ULL, 0x7e42f8ec980980e9ULL},
    {0x03370177d9ffc813ULL, 0x65acde118ecf01a7ULL},
    {0xc6f3ff633fecaf8fULL, 0x100bff64a422f567ULL},
    {0xd233b6dc93008ed5ULL, 0xb7a51d9f48028939ULL},
    {0x145bc87100617b88ULL, 0x5e87c4cef515860dULL},
    {0x8e63ce253b5f76e7ULL, 0xfbcbcfa6e6aca2a7ULL},
    {0xdfeea7ab59c25736ULL, 0x1fcce2d5b25739afULL},
    {0x7b2c5710ba0c2c8eULL, 0xb44f97434eca9a47ULL},
    {0xf05d199084455939ULL, 0x2de37a1352b2be12ULL},
    {0x48a67a0cb723e171ULL, 0x1ccd942647fefc26ULL}};
#endif
#elif (defined(HTS_INTHASH_USES_MURMUR))
#if (COUCAL_HASH_SIZE == 32)
#define KAT_VECTORS
static const uint64_t kat_vectors[][2] = {
    {0x3aa5200cUL, 0x00000000UL}, {0xfd3673c3UL, 0x00000000UL},
    {0x8ae9d282UL, 0x00000000UL}, {0x4389f567UL, 0x0601ed3cUL},
    {0x779e50e4UL, 0xdd672d9cUL}, {0xde07a68fUL, 0xa24806bdUL},
    {0x7de97cf6UL, 0xb3b5376fUL}, {0x64ecc591UL, 0x5af5e487UL},
    {0x7f4b072fUL, 0xb4033332UL}, {0x0afda147UL, 0x0c0bb6d5UL}};
#else
#define KAT_VECTORS
static const uint64_t kat_vectors[][2] = {
    {0x95c80cbaaf6d2cb6ULL, 0x95c80cba95c80cbaULL},
    {0xc823081235157bd1ULL, 0xc8230812c8230812ULL},
    {0x3ad28e70b03b5cf2ULL, 0x3ad28e703ad28e70ULL},
    {0x882f036680412724ULL, 0x8e2eee5ac3c8d243ULL},
    {0x62b4501328e77eb7ULL, 0xbfd37d8f5f792e53ULL},
    {0xa725b5bd4aa2dc25ULL, 0x056db30094a57aaaULL},
    {0x872eed54a35d8b40ULL, 0x349bda3bdeb4f7b6ULL},
    {0x029c3adcdb543fa5ULL, 0x5869de5bbfb8fa34ULL},
    {0x755be581d8a4511cULL, 0xc158d6b3a7ef5633ULL},
    {0xb2f9883d0c611652ULL, 0xbef23ee8069cb715ULL}};
#endif
#elif (defined(HTS_INTHASH_USES_FNV1))
/* FNV-1 folds bytes, so unlike the others its vectors hold on any byte order */
#define KAT_BYTE_ORDER_AGNOSTIC
#if (COUCAL_HASH_SIZE == 32)
#define KAT_VECTORS
static const uint64_t kat_vectors[][2] = {
    {0x4fd0bfc1UL, 0xb02f403eUL}, {0x29620a98UL, 0x29620729UL},
    {0x8a72ee46UL, 0x6c07f166UL}, {0xd8bfa3aeUL, 0xce1e727fUL},
    {0x8a24f51aUL, 0xabb066bbUL}, {0xdfa36ca7UL, 0x01f83463UL},
    {0x10e2ba78UL, 0x01342177UL}, {0x95651530UL, 0x83658d24UL},
    {0x0550b2dbUL, 0xde3c677bUL}, {0x8df158b0UL, 0xc61343bdUL}};
#else
#define KAT_VECTORS
static const uint64_t kat_vectors[][2] = {
    {0xcbf29ce484222325ULL, 0x340d631b7bdddcdaULL},
    {0xaf63bd4c8601b7d4ULL, 0x509c41b379fe469aULL},
    {0xed39da7f674b3439ULL, 0x8efaf978e2fd081eULL},
    {0x6538d35fbd8770f1ULL, 0xacad5e6e62b32c11ULL},
    {0x87001caf0d24e9b5ULL, 0x1dc38691b673e02aULL},
    {0x8a1a727355b91ed4ULL, 0x071dd39906e5e7faULL},
    {0x9900f4c989e24eb1ULL, 0xb02be8c6b11fc9b1ULL},
    {0xdeeea3754b8bb645ULL, 0x7a663a9ef903b7baULL},
    {0x5b36054f5e66b794ULL, 0xff6d56212151315aULL},
    {0x7e6a46d5f39b1e65ULL, 0x0e026227c811219aULL}};
#endif
#endif

static int kat_vectors_apply(void) {
#ifdef KAT_VECTORS
#ifdef KAT_BYTE_ORDER_AGNOSTIC
  return 1;
#else
  const uint32_t one = 1;
  return *(const unsigned char *) &one == 1;
#endif
#else
  return 0;
#endif
}

static int coucal_test_hash_vectors(void) {
  const int verified = kat_vectors_apply();
  unsigned char buf[64];
  size_t i;

  if (!verified) {
    fprintf(stderr, "WARNING: no known-answer vectors for this backend, hash "
                    "size or byte order: coucal_hash_data() is UNVERIFIED\n");
  }
  for (i = 0; i < sizeof(buf); i++) {
    buf[i] = (unsigned char) (i * 37 + 11);
  }
  for (i = 0; i < sizeof(kat_lengths) / sizeof(kat_lengths[0]); i++) {
    const coucal_hashkeys hashes = coucal_hash_data(buf, kat_lengths[i]);
#ifdef KAT_VECTORS
    if (verified) {
      CHECK((uint64_t) hashes.hash1 == kat_vectors[i][0]);
      CHECK((uint64_t) hashes.hash2 == kat_vectors[i][1]);
      continue;
    }
#endif
    /* the weakest claim still worth making without vectors */
    CHECK(coucal_hash_data(buf, kat_lengths[i]).hash1 == hashes.hash1);
    CHECK(hashes.hash1 != hashes.hash2);
  }
  return EXIT_SUCCESS;
}

/* Differential test: drive coucal and a trivial reference model with the same
   deterministic pseudo-random op stream and assert they never diverge. The
   key space is bounded so inserts, replaces, removes and lookups all collide
   heavily. The (bytes -> ops) shape is reused as the libFuzzer harness core. */
static uint32_t oracle_rng;
static uint32_t oracle_next(void) {
  oracle_rng = oracle_rng * 1103515245u + 12345u;
  return oracle_rng >> 1;
}

static int coucal_test_oracle(void) {
#define ORACLE_KEYS 512u
#define ORACLE_OPS  20000
  coucal h = coucal_new(0);
  intptr_t *mval = (intptr_t *) calloc(ORACLE_KEYS, sizeof(*mval));
  unsigned char *mpresent = (unsigned char *) calloc(ORACLE_KEYS, 1);
  size_t model_count = 0;
  int i;

  CHECK(mval != NULL && mpresent != NULL);
  oracle_rng = 0x01234567u;

  for (i = 0; i < ORACLE_OPS; i++) {
    uint32_t id = oracle_next() % ORACLE_KEYS;
    uint32_t op = oracle_next() % 3u;
    char key[24];
    snprintf(key, sizeof(key), "okey_%u", (unsigned) id);

    if (op == 0u) {                       /* insert or replace */
      intptr_t v = (intptr_t) (oracle_next() | 1u);   /* keep it non-zero */
      int added = coucal_write(h, key, v);
      if (!mpresent[id]) {
        CHECK(added != 0);
        mpresent[id] = 1;
        model_count++;
      } else {
        CHECK(added == 0);
      }
      mval[id] = v;
    } else if (op == 1u) {                /* remove */
      int removed = coucal_remove(h, key);
      if (mpresent[id]) {
        CHECK(removed != 0);
        mpresent[id] = 0;
        model_count--;
      } else {
        CHECK(removed == 0);
      }
    } else {                             /* lookup */
      intptr_t got;
      CHECK((coucal_exists(h, key) != 0) == (mpresent[id] != 0));
      if (mpresent[id]) {
        CHECK(coucal_read(h, key, &got) && got == mval[id]);
      }
    }
    CHECK(coucal_nitems(h) == model_count);
  }

  /* enumeration must reproduce the model set exactly */
  {
    struct_coucal_enum e = coucal_enum_new(h);
    unsigned char *check = (unsigned char *) calloc(ORACLE_KEYS, 1);
    coucal_item *it;
    size_t seen = 0;
    CHECK(check != NULL);
    while ((it = coucal_enum_next(&e)) != NULL) {
      unsigned id = ORACLE_KEYS;
      CHECK(sscanf((const char *) it->name, "okey_%u", &id) == 1
            && id < ORACLE_KEYS);
      CHECK(mpresent[id]);
      CHECK(!check[id]);
      check[id] = 1;
      CHECK(it->value.intg == mval[id]);
      seen++;
    }
    CHECK(seen == model_count);
    free(check);
  }

  coucal_delete(&h);
  free(mval);
  free(mpresent);
  return EXIT_SUCCESS;
#undef ORACLE_KEYS
#undef ORACLE_OPS
}

/* Deleting during an enumeration must not hide a surviving entry. Three keys
   per (hash1, hash2) pair, two table slots: the stash is always occupied. */
#define ENUM_DEL_KEYS 12
#define ENUM_DEL_STASHED 4
#define ENUM_DEL_FILLER 100
#define ENUM_DEL_FILLERS 5
static coucal_hashkeys enum_del_hash(coucal_opaque arg, coucal_key_const name) {
  coucal_hashkeys k;
  const int id = atoi((const char *) name + 1);
  (void) arg;
  /* small values, so the positions survive the table doublings */
  if (id < ENUM_DEL_FILLER) {
    k.hash1 = (coucal_hashkey) ((id % 4) * 3);
    k.hash2 = (coucal_hashkey) ((id % 4) * 3 + 1);
  } else {
    /* free slots: fillers grow the table without touching the stash */
    k.hash1 = (coucal_hashkey) (12 + (id - ENUM_DEL_FILLER) * 2);
    k.hash2 = (coucal_hashkey) (13 + (id - ENUM_DEL_FILLER) * 2);
  }
  return k;
}

/* coucal_delete() logs the summary before releasing anything: the only public
   window onto stash.size. */
static size_t enum_del_stash_size;
static void enum_del_log(coucal_opaque arg, coucal_loglevel level,
                         const char *format, va_list args) {
  char line[1024];
  const char *p;
  (void) arg;
  (void) level;
  vsnprintf(line, sizeof(line), format, args);
  p = strstr(line, " stash-size=");
  if (p != NULL) {
    enum_del_stash_size = (size_t) atol(p + sizeof(" stash-size=") - 1);
  }
}

static coucal enum_del_fill(void) {
  coucal h = coucal_new(0);
  int i;

  coucal_value_set_key_handler(h, NULL, NULL, enum_del_hash, NULL, NULL);
  coucal_set_assert_handler(h, enum_del_log, NULL, NULL);
  for (i = 0; i < ENUM_DEL_KEYS; i++) {
    char key[16];
    snprintf(key, sizeof(key), "k%d", i);
    coucal_write(h, key, i);
  }
  return h;
}

/* Collect the names in enumeration order ; -1 on a hole or an overflow. */
static int enum_del_order(coucal h, char order[][16], int max) {
  struct_coucal_enum e = coucal_enum_new(h);
  coucal_item *it;
  int n = 0;

  while ((it = coucal_enum_next(&e)) != NULL) {
    if (it->name == NULL || n == max) {
      return -1;
    }
    snprintf(order[n++], 16, "%s", (const char *) it->name);
  }
  return n;
}

static int enum_del_index(char order[][16], int n, const char *name) {
  int i;

  for (i = 0; i < n; i++) {
    if (strcmp(order[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

static int coucal_test_enum_delete(void) {
  coucal h;
  struct_coucal_enum e;
  coucal_item *it;
  int seen[ENUM_DEL_KEYS];
  char victim[16];
  char kept[16];
  int i, n;

  /* an empty stash would degrade every sub-test to a plain table walk */
  enum_del_stash_size = 0;
  h = enum_del_fill();
  CHECK(coucal_nitems(h) == ENUM_DEL_KEYS);
  coucal_delete(&h);
  CHECK(!STATS_LOGGED || enum_del_stash_size == ENUM_DEL_STASHED);

  h = enum_del_fill();
  memset(seen, 0, sizeof(seen));
  victim[0] = '\0';
  n = 0;
  e = coucal_enum_new(h);
  while ((it = coucal_enum_next(&e)) != NULL) {
    const int id = atoi((const char *) it->name + 1);
    CHECK(id >= 0 && id < ENUM_DEL_KEYS);
    CHECK(!seen[id]);
    seen[id] = 1;
    if (++n == 1) {
      snprintf(victim, sizeof(victim), "%s", (const char *) it->name);
    } else if (n == 3) {
      CHECK(coucal_remove(h, victim));
    }
  }
  CHECK(n == ENUM_DEL_KEYS);
  for (i = 0; i < ENUM_DEL_KEYS; i++) {
    CHECK(seen[i]);
  }
  CHECK(coucal_nitems(h) == ENUM_DEL_KEYS - 1);
  coucal_delete(&h);

  /* a write that only replaces must not promote a stashed entry either */
  h = enum_del_fill();
  memset(seen, 0, sizeof(seen));
  victim[0] = kept[0] = '\0';
  n = 0;
  e = coucal_enum_new(h);
  while ((it = coucal_enum_next(&e)) != NULL) {
    const int id = atoi((const char *) it->name + 1);
    CHECK(id >= 0 && id < ENUM_DEL_KEYS);
    CHECK(!seen[id]);
    seen[id] = 1;
    if (++n == 1) {
      snprintf(victim, sizeof(victim), "%s", (const char *) it->name);
    } else if (n == 2) {
      snprintf(kept, sizeof(kept), "%s", (const char *) it->name);
    } else if (n == 3) {
      CHECK(coucal_remove(h, victim));
      CHECK(coucal_write(h, kept, -1) == 0);
    }
  }
  CHECK(n == ENUM_DEL_KEYS);
  for (i = 0; i < ENUM_DEL_KEYS; i++) {
    CHECK(seen[i]);
  }
  coucal_delete(&h);

  h = enum_del_fill();
  memset(seen, 0, sizeof(seen));
  n = 0;
  e = coucal_enum_new(h);
  while ((it = coucal_enum_next(&e)) != NULL) {
    char key[16];
    int id;
    /* the remove frees the name, so work on a copy */
    snprintf(key, sizeof(key), "%s", (const char *) it->name);
    id = atoi(key + 1);
    CHECK(id >= 0 && id < ENUM_DEL_KEYS);
    CHECK(!seen[id]);
    seen[id] = 1;
    n++;
    CHECK(coucal_remove(h, key));
  }
  CHECK(n == ENUM_DEL_KEYS);
  CHECK(coucal_nitems(h) == 0);
  coucal_delete(&h);

  return EXIT_SUCCESS;
}

/* A deletion in the middle of the stash leaves a hole every other stash walk
   must step over. */
static int coucal_test_sparse_stash(void) {
#define ENUM_DEL_ORDER (ENUM_DEL_KEYS + ENUM_DEL_FILLERS)
  coucal h = enum_del_fill();
  char order[ENUM_DEL_ORDER][16];
  char below[16], victim[16], above[16];
  const int first = ENUM_DEL_KEYS - ENUM_DEL_STASHED;
  intptr_t value;
  int i, n;

  /* the stash comes last: take a victim with a survivor on either side */
  n = enum_del_order(h, order, ENUM_DEL_ORDER);
  CHECK(n == ENUM_DEL_KEYS);
  snprintf(below, sizeof(below), "%s", order[first]);
  snprintf(victim, sizeof(victim), "%s", order[first + 1]);
  snprintf(above, sizeof(above), "%s", order[first + 2]);
  CHECK(coucal_remove(h, victim));
  CHECK(coucal_nitems(h) == ENUM_DEL_KEYS - 1);

  for (i = 0; i < ENUM_DEL_KEYS; i++) {
    char key[16];
    snprintf(key, sizeof(key), "k%d", i);
    if (strcmp(key, victim) != 0) {
      CHECK(coucal_read(h, key, &value));
      CHECK(value == i);
    }
  }

  n = enum_del_order(h, order, ENUM_DEL_ORDER);
  CHECK(n == ENUM_DEL_KEYS - 1);
  CHECK(enum_del_index(order, n, victim) == -1);

  /* the next stashed entry reuses the hole, back between its two neighbours */
  CHECK(coucal_write(h, "k12", 12));
  n = enum_del_order(h, order, ENUM_DEL_ORDER);
  CHECK(n == ENUM_DEL_KEYS);
  CHECK(enum_del_index(order, n, above) == enum_del_index(order, n, below) + 2);

  /* rehash with a hole in the stash */
  CHECK(coucal_remove(h, above));
  for (i = 0; i < ENUM_DEL_FILLERS; i++) {
    char key[16];
    snprintf(key, sizeof(key), "k%d", ENUM_DEL_FILLER + i);
    CHECK(coucal_write(h, key, ENUM_DEL_FILLER + i));
  }
  CHECK(coucal_nitems(h) == ENUM_DEL_KEYS - 1 + ENUM_DEL_FILLERS);
  n = enum_del_order(h, order, ENUM_DEL_ORDER);
  CHECK(n == (int) coucal_nitems(h));

  enum_del_stash_size = 0;
  coucal_delete(&h);
  CHECK(!STATS_LOGGED || enum_del_stash_size != 0);

  return EXIT_SUCCESS;
#undef ENUM_DEL_ORDER
#undef ENUM_DEL_FILLERS
#undef ENUM_DEL_FILLER
#undef ENUM_DEL_STASHED
#undef ENUM_DEL_KEYS
}

int main(int argc, char **argv) {
  if (coucal_test_high_bytes() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (coucal_test_api() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (coucal_test_pool_alias() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (coucal_test_new_size() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (coucal_test_value_handler() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (coucal_test_key_handler() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (coucal_test_hardening() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (coucal_test_hash_vectors() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (coucal_test_oracle() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (coucal_test_enum_delete() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (coucal_test_sparse_stash() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (argc == 2) {
    return coucal_test(argv[1]);
  } else {
    fprintf(stderr, "usage: %s [number-of-tests | keys-filename]\n", argv[0]);
    return EXIT_FAILURE;
  }
}
