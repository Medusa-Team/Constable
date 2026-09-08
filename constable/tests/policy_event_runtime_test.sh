#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

set -eu

constable=${1:-../constable}
fixtures=${2:-fixtures}
temporary=${TMPDIR:-/tmp}/constable-policy-event-runtime.$$

cleanup()
{
	rm -f "$temporary.pass" "$temporary.historical" "$temporary.missing" \
		"$temporary.missing-historical" "$temporary.comm"
}
trap cleanup EXIT HUP INT TERM

if ! "$constable" -E semantic-test \
	-c "$fixtures/policy-event-runtime.conf" "$fixtures/offline.conf" \
	>"$temporary.pass" 2>&1
then
	echo "policy event runtime: controlled event execution failed" >&2
	sed -n '1,120p' "$temporary.pass" >&2
	exit 1
fi
grep -Fxq \
	'Policy event self-test allow-phase: status=0 result=1 trace=123456 subject=16 object=11' \
	"$temporary.pass" || {
	echo "policy event runtime: allow-phase semantics changed" >&2
	sed -n '1,120p' "$temporary.pass" >&2
	exit 1
}
grep -Fxq \
	'Policy event self-test deny-phase: status=0 result=0 trace=756 subject=15 object=4' \
	"$temporary.pass" || {
	echo "policy event runtime: deny-phase semantics changed" >&2
	sed -n '1,120p' "$temporary.pass" >&2
	exit 1
}

if ! "$constable" -H semantic-test -c "../../constable/etc/medusa.conf" \
	"$fixtures/offline.conf" >"$temporary.historical" 2>&1
then
	echo "policy event runtime: historical getfile execution failed" >&2
	sed -n '1,160p' "$temporary.historical" >&2
	exit 1
fi
grep -Fxq \
	'Policy historical event self-test: bin=fs/bin ping=fs/bin/ping status=0/0/0 result=3/3/3 pcap_net_raw=1 other_pcap_clear=1' \
	"$temporary.historical" || {
	echo "policy event runtime: historical getfile semantics changed" >&2
	sed -n '1,160p' "$temporary.historical" >&2
	exit 1
}

if "$constable" -E semantic-test \
	-c "$fixtures/policy-valid.conf" "$fixtures/offline.conf" \
	>"$temporary.missing" 2>&1
then
	echo "policy event runtime: missing _debug_event unexpectedly passed" >&2
	exit 1
fi
grep -Fq 'Policy event self-test requires event _debug_event' \
	"$temporary.missing" || {
	echo "policy event runtime: missing-event diagnostic is absent" >&2
	exit 1
}

if "$constable" -H semantic-test \
	-c "$fixtures/policy-event-runtime.conf" "$fixtures/offline.conf" \
	>"$temporary.missing-historical" 2>&1
then
	echo "policy event runtime: missing getfile unexpectedly passed" >&2
	exit 1
fi
grep -Fq 'Historical event self-test requires event getfile' \
	"$temporary.missing-historical" || {
	echo "policy event runtime: missing historical-event diagnostic is absent" >&2
	exit 1
}

if "$constable" -E absent-comm \
	-c "$fixtures/policy-event-runtime.conf" "$fixtures/offline.conf" \
	>"$temporary.comm" 2>&1
then
	echo "policy event runtime: absent comm unexpectedly passed" >&2
	exit 1
fi
grep -Fq "Policy event self-test cannot find comm 'absent-comm'" \
	"$temporary.comm" || {
	echo "policy event runtime: absent-comm diagnostic is missing" >&2
	exit 1
}

echo "policy event runtime: controlled and historical precedence, mutations, ordering, routing, and validation verified"
