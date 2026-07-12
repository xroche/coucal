#!/bin/sh
#
# Build the coucal libFuzzer harness and replay the committed seed corpus under
# ASan + UBSan. Replay (no mutation) keeps CI deterministic and pins the seeds;
# open-ended discovery is a manual / OSS-Fuzz activity.
#
# Usage: fuzz/run-fuzzers.sh            (CC=clang by default)
#        CC=clang-18 fuzz/run-fuzzers.sh
set -eu

CC=${CC:-clang}
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

cflags=${CFLAGS:-"-O1 -g -fno-omit-frame-pointer"}
cppflags="-D_REENTRANT -DHTS_INTHASH_USES_MURMUR -I."
san="-fsanitize=address,undefined,fuzzer"

echo "building coucal_fuzz with ${CC} ..."
# shellcheck disable=SC2086
"${CC}" ${cppflags} ${cflags} ${san} fuzz/coucal_fuzz.c coucal.c -o coucal_fuzz

echo "replaying $(find fuzz/corpus -type f | wc -l) corpus file(s) ..."
# Passing the corpus files runs each once and exits (replay mode, no mutation).
./coucal_fuzz fuzz/corpus/*
echo "corpus replay OK"
