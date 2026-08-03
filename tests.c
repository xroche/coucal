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
  CHECK(enum_del_stash_size == ENUM_DEL_STASHED);

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
  CHECK(enum_del_stash_size != 0);

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
  if (coucal_test_value_handler() != EXIT_SUCCESS) {
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
