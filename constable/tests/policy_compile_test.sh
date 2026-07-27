#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

set -eu

constable=${1:-../constable}
fixtures=${2:-fixtures}
temporary=${TMPDIR:-/tmp}/constable-policy-test.$$

cleanup()
{
	rm -f "$temporary.valid" "$temporary.invalid" "$temporary.duplicate" \
		"$temporary.overlong"
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

if "$constable" -t -c "$fixtures/policy-duplicate-function.conf" \
	"$fixtures/offline.conf" >"$temporary.duplicate" 2>&1; then
	echo "duplicate function unexpectedly compiled" >&2
	exit 1
fi

if ! grep -Fq 'Duplicate definition of function duplicate' \
	"$temporary.duplicate"; then
	echo "duplicate function did not produce the expected diagnostic" >&2
	sed -n '1,120p' "$temporary.duplicate" >&2
	exit 1
fi

if "$constable" -t -c "$fixtures/policy-overlong-handler.conf" \
	"$fixtures/offline.conf" >"$temporary.overlong" 2>&1; then
	echo "overlong handler name unexpectedly compiled" >&2
	exit 1
fi

if ! grep -Fq "Handler name 'func:handler_name_that_exceeds_the_protocol_field' is too long" \
	"$temporary.overlong"; then
	echo "overlong handler name did not produce the expected diagnostic" >&2
	sed -n '1,120p' "$temporary.overlong" >&2
	exit 1
fi

echo "policy compiler: valid corpus accepted; invalid, duplicate, and overlong definitions rejected"
