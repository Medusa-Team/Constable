#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

set -eu

constable=${1:-../constable}
fixtures=${2:-fixtures}
temporary=${TMPDIR:-/tmp}/constable-policy-test.$$

cleanup()
{
	rm -f "$temporary.valid" "$temporary.invalid"
}
trap cleanup EXIT HUP INT TERM

if ! "$constable" -t -c "$fixtures/policy-valid.conf" \
	"$fixtures/offline.conf" >"$temporary.valid" 2>&1; then
	echo "valid policy failed to compile" >&2
	sed -n '1,120p' "$temporary.valid" >&2
	exit 1
fi

if "$constable" -t -c "$fixtures/policy-invalid.conf" \
	"$fixtures/offline.conf" >"$temporary.invalid" 2>&1; then
	echo "invalid policy unexpectedly compiled" >&2
	exit 1
fi

if ! grep -Eq 'Missing ;|Unexpected }' "$temporary.invalid"; then
	echo "invalid policy did not produce the frozen syntax diagnostic" >&2
	sed -n '1,120p' "$temporary.invalid" >&2
	exit 1
fi

echo "policy compiler: valid corpus accepted; invalid corpus rejected"
