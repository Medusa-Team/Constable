#!/bin/sh

set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_root=$(CDPATH= cd -- "$test_dir/../.." && pwd)

fail()
{
	echo "retired-components: $*" >&2
	exit 1
}

for retired_path in \
	"$source_root/libmcompiler/compiler/c_preprocesor.c" \
	"$source_root/libmcompiler/compiler/test_c_lex.c" \
	"$source_root/libmcompiler/compiler/test_c_pre.c"
do
	test ! -e "$retired_path" ||
		fail "unsupported source returned: $retired_path"
done

if test -d "$source_root/constable/force" &&
	find "$source_root/constable/force" -type f -print -quit | grep -q .
then
	fail "unsupported force-loader source returned"
fi

if grep -q 'c_preprocessor_create' \
	"$source_root/libmcompiler/mcompiler/c_language.h"
then
	fail "retired C preprocessor remains in the public API"
fi

if grep -Eq '^[[:space:]]*MODULES[[:space:]]*=.*[[:space:]]force([[:space:]]|$)' \
	"$source_root/constable/Makefile"
then
	fail "force loader remains in the supported module list"
fi

echo "retired-components: 6 checks passed"
