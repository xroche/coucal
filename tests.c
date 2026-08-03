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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "coucal.h"

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
        const long expected = (long) i * 1664525 + 1013904223;
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
  if (coucal_test_oracle() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (argc == 2) {
    return coucal_test(argv[1]);
  } else {
    fprintf(stderr, "usage: %s [number-of-tests | keys-filename]\n", argv[0]);
    return EXIT_FAILURE;
  }
}
