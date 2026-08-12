#!/usr/bin/env bash
# Static gates for CI and local use. Run from the repo root:
#   bash tools/ci/checks.sh
#
# These encode the Enforce Script failure modes that the PBO packer does not
# catch, plus repo hygiene. They are necessary, not sufficient: only a server
# boot proves the code compiles in-engine (see CONTRIBUTING.md).
set -u
fail=0

note() { echo "[checks] $1"; }
err()  { echo "[checks] FAIL: $1"; fail=1; }

# All Enforce Script source: the core PBO tree plus every addon PBO tree.
list_script_files() {
    find scripts addons -name '*.c' 2>/dev/null
    ls config.cpp 2>/dev/null
    find addons -name 'config.cpp' 2>/dev/null
}

# --- 1. ASCII-only source files -------------------------------------------
# Non-ASCII bytes in .c/.cpp break the in-engine tokenizer.
while IFS= read -r f; do
    if LC_ALL=C grep -nP '[^\x00-\x7F]' "$f" > /dev/null 2>&1; then
        err "non-ASCII bytes in $f:"
        LC_ALL=C grep -nP '[^\x00-\x7F]' "$f" | head -5 | sed 's/^/    /'
    fi
done < <(list_script_files)

# --- 2. No leading-+ string continuation ----------------------------------
# A continuation line starting with + is an in-engine syntax error.
while IFS= read -r f; do
    if grep -nE '^[[:space:]]+\+[[:space:]]*"' "$f" > /dev/null 2>&1; then
        err "leading-+ string continuation in $f (put + at end of previous line):"
        grep -nE '^[[:space:]]+\+[[:space:]]*"' "$f" | head -5 | sed 's/^/    /'
    fi
done < <(find scripts addons -name '*.c' 2>/dev/null)

# --- 3. Version lockstep: DmVersion.c <-> config.cpp ----------------------
ver_code=$(grep -oE 'VERSION = "[0-9]+\.[0-9]+\.[0-9]+"' scripts/3_Game/DmVersion.c 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
ver_cfg=$(grep -oE 'version = "[0-9]+\.[0-9]+\.[0-9]+"' config.cpp 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
if [ -z "$ver_code" ] || [ -z "$ver_cfg" ]; then
    err "could not extract versions (DmVersion.c: '${ver_code:-none}', config.cpp: '${ver_cfg:-none}')"
elif [ "$ver_code" != "$ver_cfg" ]; then
    err "version drift: DmVersion.c=$ver_code config.cpp=$ver_cfg"
else
    note "version lockstep OK ($ver_code)"
fi

# --- 4. Single chain link per modded class, PER PBO -----------------------
# The rule is per compiled PBO: the core (scripts/) and each addons/<name>/
# are separate PBOs, each allowed exactly one block per foreign type.
check_chain_links() {
    tree="$1"
    for cls in PlayerBase MissionServer MissionGameplay; do
        count=$(grep -rlE "modded[[:space:]]+class[[:space:]]+$cls([[:space:]]|$)" "$tree" 2>/dev/null | wc -l)
        if [ "$count" -gt 1 ]; then
            err "multiple 'modded class $cls' blocks in PBO tree $tree ($count) - consolidate into one:"
            grep -rlE "modded[[:space:]]+class[[:space:]]+$cls([[:space:]]|$)" "$tree" | sed 's/^/    /'
        fi
    done
}
check_chain_links scripts
while IFS= read -r addonTree; do
    check_chain_links "$addonTree"
done < <(find addons -mindepth 1 -maxdepth 1 -type d 2>/dev/null)
note "chain-link check done"

# --- 5. No OnUpdate overrides ---------------------------------------------
if grep -rnE 'override[[:space:]]+void[[:space:]]+OnUpdate' scripts addons > /dev/null 2>&1; then
    err "OnUpdate override found - this mod does no per-frame work (CONTRIBUTING.md):"
    grep -rnE 'override[[:space:]]+void[[:space:]]+OnUpdate' scripts addons | sed 's/^/    /'
fi

# --- 6. Fixture coverage: every 4_World service registers a SelfTest ------
while IFS= read -r f; do
    base=$(basename "$f" .c)
    if ! grep -qE 'static[[:space:]]+void[[:space:]]+SelfTest' "$f"; then
        err "$base has no SelfTest() fixture (testing standard: every subsystem ships one)"
    elif ! grep -q "$base.SelfTest" scripts/5_Mission/DmMissionServer.c 2>/dev/null; then
        err "$base.SelfTest() is not registered in DmRunSelfTests()"
    fi
done < <(find scripts/4_World -maxdepth 1 -name 'Dm*.c' 2>/dev/null)
note "fixture coverage check done"

# --- 7. Workshop description budget (Steam hard cap 8000; budget 7900) ----
if [ -d workshop ]; then
    while IFS= read -r f; do
        len=$(tr -d '\r' < "$f" | wc -c)
        if [ "$len" -gt 7900 ]; then
            err "$f is $len chars - exceeds the 7900-char budget (Steam caps at 8000 and silently rejects)"
        fi
    done < <(find workshop -name 'description.bbcode' 2>/dev/null)
fi

# --- 8. Example configs are valid JSON ------------------------------------
# Skipped when no working python3 is on PATH (e.g. bare Windows dev boxes);
# CI's ubuntu runner always has one, so the gate is never skipped in CI.
if python3 -c "print(1)" > /dev/null 2>&1; then
    while IFS= read -r f; do
        if ! python3 -m json.tool "$f" > /dev/null 2>&1; then
            err "invalid JSON: $f"
        fi
    done < <(find docs/examples -name '*.json' 2>/dev/null)
else
    note "python3 unavailable - JSON validation skipped locally (CI still enforces it)"
fi

if [ "$fail" -ne 0 ]; then
    echo "[checks] FAILED"
    exit 1
fi
echo "[checks] all static gates passed"
