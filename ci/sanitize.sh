#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

set -eu

case ${1:-} in
address)
	sanitizer_flags="-fsanitize=address"
	runtime_options="detect_leaks=1:halt_on_error=1"
	;;
undefined)
	sanitizer_flags="-fsanitize=undefined"
	runtime_options="halt_on_error=1:print_stacktrace=1"
	;;
*)
	echo "usage: $0 address|undefined" >&2
	exit 2
	;;
esac

repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
compiler=${CC:-gcc}
common_flags="-std=c11 -Wall -Wextra -Wpedantic -pedantic-errors -Werror -g -D_GNU_SOURCE"
lib_flags="$common_flags -fPIC $sanitizer_flags -fno-omit-frame-pointer"
constable_flags="$common_flags -DRBAC -I$repo/libmcompiler \
$sanitizer_flags -fno-omit-frame-pointer"
constable_ldflags="../libmcompiler/libmcompiler.a -pthread $sanitizer_flags"

cd "$repo"
make -C constable clean
make -C libmcompiler clean
make -C libmcompiler \
	CC="$compiler" \
	LD="$compiler" \
	CFLAGS="$lib_flags" \
	LDFLAGS="$sanitizer_flags"
make -C constable \
	CC="$compiler" \
	CFLAGS="$constable_flags" \
	LDFLAGS="$constable_ldflags"

ASAN_OPTIONS=$runtime_options \
UBSAN_OPTIONS=$runtime_options \
make -C constable test \
	CC="$compiler" \
	CFLAGS="$constable_flags" \
	LDFLAGS="$constable_ldflags"
