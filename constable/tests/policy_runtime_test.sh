#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

set -eu

constable=${1:-../constable}
fixtures=${2:-fixtures}
temporary=${TMPDIR:-/tmp}/constable-policy-runtime.$$

cleanup()
{
	rm -f "$temporary.pass" "$temporary.deny" "$temporary.missing" "$temporary.arithmetic" "$temporary.conf"
}
trap cleanup EXIT HUP INT TERM

if ! "$constable" -T -c "$fixtures/policy-runtime.conf" \
	"$fixtures/offline.conf" >"$temporary.pass" 2>&1
then
	echo "policy runtime: passing self-test failed" >&2
	sed -n '1,120p' "$temporary.pass" >&2
	exit 1
fi
if ! grep -Fxq 'Policy self-test result: 0' "$temporary.pass"; then
	echo "policy runtime: passing result was not reported" >&2
	sed -n '1,120p' "$temporary.pass" >&2
	exit 1
fi

if "$constable" -T -c "$fixtures/policy-runtime-deny.conf" \
	"$fixtures/offline.conf" >"$temporary.deny" 2>&1
then
	echo "policy runtime: denying self-test unexpectedly succeeded" >&2
	exit 1
fi
if ! grep -Fq 'Policy self-test did not return FORCE_ALLOW' \
	"$temporary.deny"; then
	echo "policy runtime: denial did not produce the expected diagnostic" >&2
	sed -n '1,120p' "$temporary.deny" >&2
	exit 1
fi

if "$constable" -T -c "$fixtures/policy-valid.conf" \
	"$fixtures/offline.conf" >"$temporary.missing" 2>&1
then
	echo "policy runtime: missing _debug unexpectedly succeeded" >&2
	exit 1
fi
if ! grep -Fq 'Policy self-test requires function _debug' \
	"$temporary.missing"; then
	echo "policy runtime: missing _debug did not produce the expected diagnostic" >&2
	sed -n '1,120p' "$temporary.missing" >&2
	exit 1
fi

echo "policy runtime: compiled calls, control flow, pass, deny, and missing entry point verified"

# Errors inside nested calls must unwind and deny, rather than reach the
# unconditional FORCE_ALLOW after the expression or crash the server.
for expression in '7 / 0' '7 % 0' '1 << 64' '1 << (0 - 1)' '0xffffffffffffffff + 1'
do
	cat >"$temporary.conf" <<EOF
function invalid_arithmetic
{
	return $expression;
}
function outer
{
	invalid_arithmetic();
	return FORCE_ALLOW;
}
function _debug
{
	outer();
	return FORCE_ALLOW;
}
EOF
	if "$constable" -T -c "$temporary.conf" "$fixtures/offline.conf" \
		>"$temporary.arithmetic" 2>&1
	then
		echo "policy runtime: invalid arithmetic unexpectedly allowed: $expression" >&2
		exit 1
	fi
	if ! grep -Fq 'Runtime error : Invalid integer operation' "$temporary.arithmetic" ||
	   ! grep -Fq 'Policy self-test did not return FORCE_ALLOW' "$temporary.arithmetic"
	then
		echo "policy runtime: unexpected failure for $expression" >&2
		cat "$temporary.arithmetic" >&2
		exit 1
	fi
done
echo "policy runtime: arithmetic errors in nested calls deny and stop execution"
