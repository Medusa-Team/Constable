#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

set -eu

constable=${1:-../constable}
fixtures=${2:-fixtures}
temporary=${TMPDIR:-/tmp}/constable-policy-inspection.$$

cleanup()
{
	rm -f "$temporary.json" "$temporary.stdout" "$temporary.error" \
		"$temporary.modular"
}
trap cleanup EXIT HUP INT TERM

"$constable" -I "$temporary.json" -c "$fixtures/policy-valid.conf" \
	"$fixtures/offline.conf"

python3 - "$temporary.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    policy = json.load(stream)

assert policy["format"] == "constable-policy-v1"
assert {"spaces", "namespace", "events", "unreachable_rules"} <= policy.keys()

spaces = {space["name"]: space for space in policy["spaces"]}
assert spaces["domains"]["primary"] is True
assert spaces["domains"]["access"]["READ"] == ["files", "domains"]
assert spaces["files"]["members"] == 4
assert "?@2" in spaces
assert len(spaces) == len(policy["spaces"])

nodes = {node["path"]: node for node in policy["namespace"]}
assert nodes["domain"]["primary_space"] == "domains"
assert nodes["fs/private"]["access"]["MEMBER"] == []
assert nodes["fs/public"]["access"]["MEMBER"] == ["?@2", "files"]

events = {event["name"]: event for event in policy["events"]}
assert set(events) == {"getfile", "getprocess", "mkdir", "unlink"}
assert [rule["mode"] for rule in events["mkdir"]["rules"]] == [
    "vs_allow", "notify_deny"
]
assert events["unlink"]["rules"][0]["subject_spaces"] == ["?@2"]
assert policy["unreachable_rules"] == []
PY

"$constable" -I - -c "$fixtures/policy-valid.conf" \
	"$fixtures/offline.conf" >"$temporary.stdout"
cmp -s "$temporary.json" "$temporary.stdout" || {
	echo "policy inspection: stdout and file output differ" >&2
	exit 1
}

"$constable" -I "$temporary.modular" -c "../../constable/etc/medusa.conf" \
	"$fixtures/offline.conf"
python3 - "$temporary.modular" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    policy = json.load(stream)

names = [space["name"] for space in policy["spaces"]]
assert len(names) == len(set(names)) == 46
assert len(policy["namespace"]) == 406
assert len(policy["events"]) == 7
assert policy["unreachable_rules"] == []
PY

if "$constable" -I /dev/full -c "$fixtures/policy-valid.conf" \
	"$fixtures/offline.conf" >"$temporary.error" 2>&1
then
	echo "policy inspection: write failure unexpectedly succeeded" >&2
	exit 1
fi
grep -Fq 'Cannot write policy inspection output' "$temporary.error" || {
	echo "policy inspection: write failure diagnostic is missing" >&2
	sed -n '1,120p' "$temporary.error" >&2
	exit 1
}

echo "policy inspection: JSON schema, semantics, reachability, stdout, and write failure verified"
