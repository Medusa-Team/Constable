#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

set -eu

constable=${1:-../constable}
temporary=${TMPDIR:-/tmp}/constable-cli.$$

cleanup()
{
	rm -f "$temporary.help" "$temporary.unknown" "$temporary.missing" \
		"$temporary.workers"
}
trap cleanup EXIT HUP INT TERM

"$constable" --help >"$temporary.help" 2>&1
grep -Fq 'Usage:' "$temporary.help"
grep -Fq -- '--help' "$temporary.help"
grep -Fq -- '-c <policy file>' "$temporary.help"
grep -Fq -- '--fallback <event=policy>' "$temporary.help"
grep -Fq -- '--approval-socket <path>' "$temporary.help"
grep -Fq -- '--workers <auto|1-32>' "$temporary.help"

"$constable" -t -c fixtures/policy-valid.conf fixtures/approval.conf

if "$constable" -trash >"$temporary.unknown" 2>&1
then
	echo "cli integration: prefix-matched unknown option was accepted" >&2
	exit 1
fi
grep -Fxq 'Unknown option: -trash' "$temporary.unknown"

if "$constable" -V >"$temporary.missing" 2>&1
then
	echo "cli integration: missing option argument was accepted" >&2
	exit 1
fi
grep -Fxq 'Option -V requires an argument' "$temporary.missing"

if "$constable" -F invalid >"$temporary.missing" 2>&1
then
	echo "cli integration: malformed fallback policy was accepted" >&2
	exit 1
fi
grep -Fq 'Invalid fallback policy' "$temporary.missing"

if "$constable" --workers 33 >"$temporary.workers" 2>&1
then
	echo "cli integration: invalid worker count was accepted" >&2
	exit 1
fi
grep -Fxq 'Invalid worker count: 33 (expected auto or 1-32)' \
	"$temporary.workers"

echo "cli integration: all checks passed"
