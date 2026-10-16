#!/usr/bin/env bash
# ps5-homebrew-ui - Incremental host GoogleTest compilation.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds build/tests/unit_tests from tests/unit/*_test.cpp plus the
# platform-neutral application sources listed in tests/unit/sources.txt.

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/tools/ninja-build.sh"
gtest=$(bash "$root/tools/setup-test-dependencies.sh")
cxx=$(command -v "${HOST_CXX:-clang++}")
cc=$(command -v "${HOST_CC:-clang}")
build="$root/build/tests"
ninja_begin "$build/build.ninja"
flags=(${HOST_TEST_CXXFLAGS:--std=c++20 -O2 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections})
default_ldflags="-Wl,--gc-sections"
[[ $(uname) == "Darwin" ]] && default_ldflags="-Wl,-dead_strip"
ldflags=(${HOST_TEST_LDFLAGS:-$default_ldflags})
sanitizers=(${HOST_TEST_SANITIZERS:--fsanitize=address,undefined -fno-omit-frame-pointer})

sources=("$gtest/googletest/src/gtest-all.cc" "$gtest/googletest/src/gtest_main.cc")
while IFS= read -r -d '' test_source; do
    sources+=("$test_source")
done < <(find "$root/tests/unit" -type f -name '*_test.cpp' -print0 | sort -z)
while IFS= read -r line; do
    line=${line%%#*}
    line=${line//[[:space:]]/}
    [[ -n $line ]] || continue
    if [[ $line == *'*'* ]]; then
        mapfile -t matches < <(cd "$root" && compgen -G "$line" | sort)
        (( ${#matches[@]} )) || { echo "tests/unit/sources.txt: no match for $line" >&2; exit 2; }
        for match in "${matches[@]}"; do sources+=("$root/$match"); done
        continue
    fi
    [[ -f $root/$line ]] || { echo "tests/unit/sources.txt: missing $line" >&2; exit 2; }
    sources+=("$root/$line")
done < "$root/tests/unit/sources.txt"

objects=()
for source in "${sources[@]}"; do
    relative=${source#"$root/"}
    relative=${relative#"$gtest/"}
    object="$build/obj/${relative//\//_}.o"
    ninja_inputs=("$source" "$cxx")
    if [[ $source == "$gtest/"* ]]; then
        ninja_edge CXX "$object" "${compiler_cache[@]}" "$cxx" -std=c++20 -O2 -pthread \
            "${sanitizers[@]}" -isystem "$gtest/googletest/include" -I"$gtest/googletest" \
            -MD -MF "$object.d" -c "$source" -o "$object"
    elif [[ $source == *.c ]]; then
        # Vendored C builds without -Werror.
        ninja_inputs=("$source" "$cc")
        ninja_edge CC "$object" "${compiler_cache[@]}" "$cc" -std=c11 -O2 -w \
            -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION=1 -DSQLITE_DEFAULT_MEMSTATUS=0 \
            "${sanitizers[@]}" -I"$root/src" -MD -MF "$object.d" -c "$source" -o "$object"
    else
        ninja_edge CXX "$object" "${compiler_cache[@]}" "$cxx" "${flags[@]}" -pthread \
            "${sanitizers[@]}" "-DHUI_SOURCE_DIR=\"$root\"" -I"$root/src" -I"$root/tests" \
            -isystem "$gtest/googletest/include" -MD -MF "$object.d" -c "$source" -o "$object"
    fi
    objects+=("$object")
done
ninja_inputs=("${objects[@]}" "$cxx")
ninja_edge LINK "$build/unit_tests" "$cxx" "${flags[@]}" -pthread "${sanitizers[@]}" \
    "${objects[@]}" "${ldflags[@]}" -lm -o "$build/unit_tests"
ninja_run
