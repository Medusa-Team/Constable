#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

set -eu

constable=${1:-../constable}
fixtures=${2:-fixtures}
temporary=${TMPDIR:-/tmp}/constable-policy-runtime.$$

cleanup()
{
	rm -f "$temporary.pass" "$temporary.deny" "$temporary.missing"
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
