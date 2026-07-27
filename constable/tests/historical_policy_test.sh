#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_root=$(CDPATH= cd -- "$test_dir/../.." && pwd)
constable=${1:-"$test_dir/../constable"}
manifest="$test_dir/fixtures/historical-policies.manifest"
temporary=${TMPDIR:-/tmp}/constable-historical-policy.$$

cleanup()
{
	rm -f "$temporary.tree" "$temporary.log"
}
trap cleanup EXIT HUP INT TERM

fail()
{
	echo "historical policy: $*" >&2
	if test -s "$temporary.log"; then
		sed -n '1,120p' "$temporary.log" >&2
	fi
	exit 1
}

hash_file()
{
	if command -v sha256sum >/dev/null 2>&1; then
		sha256sum "$1" | awk '{ print $1 }'
	else
		shasum -a 256 "$1" | awk '{ print $1 }'
	fi
}

checks=0
while read -r name expected_lines expected_hash policy
do
	case "$name" in
	''|'#'*) continue ;;
	esac

	: >"$temporary.log"
	if ! "$constable" -d "$temporary.tree" \
		-c "$source_root/constable/$policy" \
		"$test_dir/fixtures/offline.conf" \
		>"$temporary.log" 2>&1
	then
		fail "$policy no longer compiles"
	fi
	test ! -s "$temporary.log" ||
		fail "$policy produced unexpected diagnostics"

	actual_lines=$(wc -l <"$temporary.tree" | tr -d '[:space:]')
	test "$actual_lines" = "$expected_lines" ||
		fail "$policy tree has $actual_lines lines, expected $expected_lines"

	actual_hash=$(hash_file "$temporary.tree")
	test "$actual_hash" = "$expected_hash" ||
		fail "$policy compiled-tree semantics changed"

	checks=$((checks + 1))
done <"$manifest"

test "$checks" -eq 5 ||
	fail "manifest contains $checks policies, expected 5"

minimal_policy="$source_root/constable/minimal/medusa.conf"
grep -Fq "transparent path_guard path_guard;" "$minimal_policy" ||
	fail "minimal policy does not declare the path_guard object"
grep -Fq "fetch path_guard;" "$minimal_policy" ||
	fail "minimal policy does not exercise path_guard fetch syntax"
grep -Fq "update path_guard;" "$minimal_policy" ||
	fail "minimal policy does not exercise path_guard update syntax"

echo "historical policy: $checks preserved policies and compiled trees match; path_guard syntax present"
