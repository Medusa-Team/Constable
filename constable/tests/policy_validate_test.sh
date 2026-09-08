#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

set -eu

constable=${1:-../constable}
fixtures=${2:-fixtures}
temporary=${TMPDIR:-/tmp}/constable-policy-validation.$$
oversized_directory=$temporary.oversized

cleanup()
{
	rm -f "$temporary.active" "$temporary.mixed" "$temporary.malformed" \
		"$temporary.malformed-class" "$temporary.duplicate-event" \
		"$temporary.duplicate-class" "$temporary.missing" \
		"$temporary.missing-class" "$temporary.network" \
		"$temporary.oversized-output"
	rm -rf "$oversized_directory"
}
trap cleanup EXIT HUP INT TERM

if ! "$constable" -t -V "$fixtures/inventory-active" \
	-c "$fixtures/policy-valid.conf" "$fixtures/offline.conf" \
	>"$temporary.active" 2>&1
then
	echo "policy validation: active inventory was rejected" >&2
	sed -n '1,120p' "$temporary.active" >&2
	exit 1
fi
grep -Fxq \
	'Policy validation events: referenced=4 active=4 announced=0 missing=0' \
	"$temporary.active" || {
	echo "policy validation: active summary is missing" >&2
	sed -n '1,120p' "$temporary.active" >&2
	exit 1
}
grep -Fxq \
	'Policy validation classes: referenced=2 active=2 announced=0 missing=0' \
	"$temporary.active" || {
	echo "policy validation: active class summary is missing" >&2
	exit 1
}

if ! "$constable" -t -V "$fixtures/inventory-network" \
	-c "$fixtures/policy-network.conf" "$fixtures/offline.conf" \
	>"$temporary.network" 2>&1
then
	echo "policy validation: active network inventory was rejected" >&2
	sed -n '1,120p' "$temporary.network" >&2
	exit 1
fi
grep -Fxq \
	'Policy validation events: referenced=7 active=7 announced=0 missing=0' \
	"$temporary.network" || {
	echo "policy validation: active network event summary is missing" >&2
	sed -n '1,120p' "$temporary.network" >&2
	exit 1
}
grep -Fxq \
	'Policy validation classes: referenced=2 active=2 announced=0 missing=0' \
	"$temporary.network" || {
	echo "policy validation: active network class summary is missing" >&2
	exit 1
}

if "$constable" -t -V "$fixtures/inventory-mixed" \
	-c "$fixtures/policy-valid.conf" "$fixtures/offline.conf" \
	>"$temporary.mixed" 2>&1
then
	echo "policy validation: mixed inventory unexpectedly passed" >&2
	exit 1
fi
grep -Fq \
	"event 'mkdir' is announced but not actively enforced (subject=process object=file)" \
	"$temporary.mixed" || {
	echo "policy validation: announcement-only diagnostic is missing" >&2
	exit 1
}
grep -Fq "event 'unlink' is missing from the kernel inventory" \
	"$temporary.mixed" || {
	echo "policy validation: missing-event diagnostic is missing" >&2
	exit 1
}
grep -Fq 'events: referenced=4 active=2 announced=1 missing=1' \
	"$temporary.mixed" || {
	echo "policy validation: mixed summary is missing" >&2
	exit 1
}
grep -Fq \
	"class 'process' is announced but has no actively enforced event" \
	"$temporary.mixed" || {
	echo "policy validation: announcement-only class diagnostic is missing" >&2
	exit 1
}
grep -Fq "class 'file' is missing from the kernel inventory" \
	"$temporary.mixed" || {
	echo "policy validation: missing-class diagnostic is missing" >&2
	exit 1
}
grep -Fq 'classes: referenced=2 active=0 announced=1 missing=1' \
	"$temporary.mixed" || {
	echo "policy validation: mixed class summary is missing" >&2
	exit 1
}

if "$constable" -t -V "$fixtures/inventory-malformed" \
	-c "$fixtures/policy-valid.conf" "$fixtures/offline.conf" \
	>"$temporary.malformed" 2>&1
then
	echo "policy validation: malformed inventory unexpectedly passed" >&2
	exit 1
fi
grep -Fq 'malformed event inventory line 1' "$temporary.malformed" || {
	echo "policy validation: malformed diagnostic is missing" >&2
	exit 1
}

if "$constable" -t -V "$fixtures/inventory-malformed-class" \
	-c "$fixtures/policy-valid.conf" "$fixtures/offline.conf" \
	>"$temporary.malformed-class" 2>&1
then
	echo "policy validation: malformed class inventory unexpectedly passed" >&2
	exit 1
fi
grep -Fq 'malformed class inventory line 1' \
	"$temporary.malformed-class" || {
	echo "policy validation: malformed class diagnostic is missing" >&2
	exit 1
}

if "$constable" -t -V "$fixtures/inventory-duplicate-event" \
	-c "$fixtures/policy-valid.conf" "$fixtures/offline.conf" \
	>"$temporary.duplicate-event" 2>&1
then
	echo "policy validation: duplicate event unexpectedly passed" >&2
	exit 1
fi
grep -Fq "duplicate event 'getfile' on line 2" \
	"$temporary.duplicate-event" || {
	echo "policy validation: duplicate event diagnostic is missing" >&2
	exit 1
}

if "$constable" -t -V "$fixtures/inventory-duplicate-class" \
	-c "$fixtures/policy-valid.conf" "$fixtures/offline.conf" \
	>"$temporary.duplicate-class" 2>&1
then
	echo "policy validation: duplicate class unexpectedly passed" >&2
	exit 1
fi
grep -Fq "duplicate class 'file' on line 2" \
	"$temporary.duplicate-class" || {
	echo "policy validation: duplicate class diagnostic is missing" >&2
	exit 1
}

if "$constable" -t -V "$fixtures/does-not-exist.inventory" \
	-c "$fixtures/policy-valid.conf" "$fixtures/offline.conf" \
	>"$temporary.missing" 2>&1
then
	echo "policy validation: absent inventory unexpectedly passed" >&2
	exit 1
fi
grep -Fq 'cannot open event inventory' "$temporary.missing" || {
	echo "policy validation: absent-file diagnostic is missing" >&2
	exit 1
}

if "$constable" -t -V "$fixtures/inventory-no-classes" \
	-c "$fixtures/policy-valid.conf" "$fixtures/offline.conf" \
	>"$temporary.missing-class" 2>&1
then
	echo "policy validation: absent class inventory unexpectedly passed" >&2
	exit 1
fi
grep -Fq 'cannot open class inventory' "$temporary.missing-class" || {
	echo "policy validation: absent class-file diagnostic is missing" >&2
	exit 1
}

mkdir "$oversized_directory"
cp "$fixtures/inventory-active/classes" "$oversized_directory/classes"
awk 'BEGIN {
	printf "event=";
	for (i = 0; i < 65536; i++)
		printf "x";
	print "";
}' >"$oversized_directory/events"
if "$constable" -t -V "$oversized_directory" \
	-c "$fixtures/policy-valid.conf" "$fixtures/offline.conf" \
	>"$temporary.oversized-output" 2>&1
then
	echo "policy validation: oversized inventory unexpectedly passed" >&2
	exit 1
fi
grep -Fq 'event inventory line 1 exceeds 65536 bytes' \
	"$temporary.oversized-output" || {
	echo "policy validation: oversized-line diagnostic is missing" >&2
	exit 1
}

echo "policy validation: active, network, announced, missing, malformed, duplicate, absent, and oversized inventories verified"
