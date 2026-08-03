###############################################################################
#
# coucal — Cuckoo-hashing hashtable with a stash area
#
###############################################################################

# --- Toolchain (overridable) -------------------------------------------------
# cc/ar are the POSIX defaults; CI overrides CC with clang or "gcc -m32".
CC      ?= cc
AR      ?= ar
RM      ?= rm -f
INSTALL ?= install
PREFIX  ?= /usr/local

# --- Flags -------------------------------------------------------------------
# CFLAGS holds the user-overridable optimization/debug flags. Everything
# mandatory (PIC, reentrancy, strict warnings, the default hash backend) is
# appended with `override` so it survives a command-line CFLAGS=... — e.g. CI
# injecting sanitizer flags.
CFLAGS  ?= -O3 -g

# --- Hash backend (overridable) ----------------------------------------------
# Which hash function coucal compiles in, and the hash-key width. The default
# (MurmurHash, 32-bit) is self-contained; CI overrides these to exercise the
# other backends and the 64-bit key path so a config only the default build
# touches cannot rot (see .github/workflows/ci.yml).
#   HASH_BACKEND: MURMUR (default) | FNV1 | OPENSSL_MD5 | MD5
#   HASH_SIZE:    32 (default) | 64
HASH_BACKEND ?= MURMUR
HASH_SIZE    ?= 32

override CPPFLAGS += -D_REENTRANT -D_GNU_SOURCE \
                    -DHTS_INTHASH_USES_$(HASH_BACKEND) \
                    -DCOUCAL_HASH_SIZE=$(HASH_SIZE)
# -Wcast-qual is implied by neither -Wall nor -Wextra, and HTTrack builds with it.
override CFLAGS   += -fPIC -pthread \
                    -W -Wall -Wextra -Werror -Wno-unused-function -Wcast-qual

# The OpenSSL MD5 backend links against libcrypto (the others are self-contained).
ifeq ($(HASH_BACKEND),OPENSSL_MD5)
  override LDLIBS += -lcrypto
endif

# --- Platform shared-library wiring ------------------------------------------
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  SHLIB   := libcoucal.dylib
  SOFLAGS := -dynamiclib -install_name @rpath/$(SHLIB)
  SOLIBS  :=
else
  SHLIB   := libcoucal.so
  SOFLAGS := -shared -Wl,-soname=$(SHLIB) -Wl,--no-undefined
  SOLIBS  := -ldl -lpthread
endif

STATICLIB := libcoucal.a

# --- Sources -----------------------------------------------------------------
LIBSRC := coucal.c
LIBOBJ := $(LIBSRC:.c=.o)
BINS   := tests sample

# --- Targets -----------------------------------------------------------------
.PHONY: all check test clean dist install
.DEFAULT_GOAL := all

all: $(STATICLIB) $(SHLIB) $(BINS)

# Every object needs the public header; the library object also pulls in the
# bundled MurmurHash implementation.
%.o: %.c coucal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
coucal.o: murmurhash3.h

$(STATICLIB): $(LIBOBJ)
	$(AR) rcs $@ $^

$(SHLIB): $(LIBOBJ)
	$(CC) $(CFLAGS) $(SOFLAGS) $(LIBOBJ) -o $@ $(LDFLAGS) $(SOLIBS) $(LDLIBS)

# tests/sample link the static archive: no LD_LIBRARY_PATH, identical run on
# Linux and macOS.
tests: tests.o $(STATICLIB)
	$(CC) $(CFLAGS) $< $(STATICLIB) -o $@ $(LDFLAGS) $(LDLIBS)
sample: sample.o $(STATICLIB)
	$(CC) $(CFLAGS) $< $(STATICLIB) -o $@ $(LDFLAGS) $(LDLIBS)

check test: tests
	./tests 100000

install: all
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include
	$(INSTALL) -m 644 $(STATICLIB) $(DESTDIR)$(PREFIX)/lib/
	$(INSTALL) -m 755 $(SHLIB)     $(DESTDIR)$(PREFIX)/lib/
	$(INSTALL) -m 644 coucal.h     $(DESTDIR)$(PREFIX)/include/

dist:
	$(RM) coucal.tgz
	tar cvfz coucal.tgz $(LIBSRC) coucal.h murmurhash3.h \
		sample.c tests.c Makefile LICENSE README.md

clean:
	$(RM) *.o $(STATICLIB) $(SHLIB) libcoucal.so.* $(BINS) coucal.tgz
