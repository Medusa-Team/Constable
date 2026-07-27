#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

set -eu

constable=${1:-../constable}
temporary=${TMPDIR:-/tmp}/constable-cli.$$

cleanup()
{
	rm -f "$temporary.help" "$temporary.unknown" "$temporary.missing"
}
trap cleanup EXIT HUP INT TERM

"$constable" --help >"$temporary.help" 2>&1
grep -Fq 'Usage:' "$temporary.help"
grep -Fq -- '--help' "$temporary.help"
grep -Fq -- '-c <policy file>' "$temporary.help"

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

echo "cli integration: all checks passed"
