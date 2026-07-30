#!/bin/sh
# Run every test in this directory and report one line each.
#
#   WYN_ROOT=/path/to/wyn ./tests/run.sh
#
# Run from the REPO ROOT, not from tests/. Two of the tests need it:
#   - test_codegen and test_pipeline generate .wyn files that `import widgets`,
#     which resolves via ./src/ relative to the CURRENT DIRECTORY, and then shell
#     out to `wyn build` on them.
# The others do not care, so making the whole script root-relative is simpler
# than making one test special.
#
# SDL_VIDEODRIVER=dummy is set here rather than left to the caller: the tests
# that need a window skip themselves without it and still report success, so
# forgetting it silently reduces coverage instead of failing.
set -e

cd "$(dirname "$0")/.."

if [ -z "$WYN_ROOT" ]; then
    echo "WYN_ROOT is not set - point it at your wyn checkout" >&2
    exit 2
fi
WYN="$WYN_ROOT/wyn"
if [ ! -x "$WYN" ]; then
    echo "no wyn binary at $WYN" >&2
    exit 2
fi

export SDL_VIDEODRIVER=dummy
export WYN_ROOT

# No `timeout` binary on macOS, so use perl's alarm. A hung test has to fail the
# run rather than hang it: these drive an event loop, and a scroll or caret bug
# is as likely to spin forever as it is to give a wrong answer.
run_one() {
    perl -e 'alarm(shift @ARGV); exec @ARGV' 300 "$WYN" run "$1" 2>&1
}

pass=0
fail=0

for t in tests/test_*.wyn; do
    name=$(basename "$t" .wyn)
    out=$(run_one "$t" || true)
    # Two report styles: the `wyn test` blocks print "N tests passed", the
    # hand-rolled harnesses print "=== ALL X TESTS PASSED ===".
    if echo "$out" | grep -q "ALL .* PASSED"; then
        n=$(echo "$out" | grep -c "^  ok " || true)
        printf '  PASS  %-18s (%s assertions)\n' "$name" "$n"
        pass=$((pass + 1))
    elif echo "$out" | grep -qE "^.*[0-9]+ tests passed"; then
        n=$(echo "$out" | grep -oE "[0-9]+ tests passed" | head -1)
        printf '  PASS  %-18s (%s)\n' "$name" "$n"
        pass=$((pass + 1))
    else
        printf '  FAIL  %-18s\n' "$name"
        echo "$out" | grep -E "FAIL|panic|Error|error:" | head -5 | sed 's/^/          /'
        fail=$((fail + 1))
    fi
done

# The examples' own headless self-tests. They are not in tests/ because they are
# also the documentation, but they assert, so leaving them out of the gate means
# the two biggest programs in the repo are unverified.
for ex in designer wizard; do
    if [ -f "examples/$ex.wyn" ]; then
        perl -e 'alarm(120); exec @ARGV' "$WYN" build "examples/$ex.wyn" >/dev/null 2>&1 || {
            printf '  FAIL  %-18s (does not build)\n' "$ex"
            fail=$((fail + 1)); continue; }
        out=$(perl -e 'alarm(shift @ARGV); exec @ARGV' 120 "./examples/$ex" --selftest 2>&1 || true)
        if echo "$out" | grep -q "FAIL"; then
            printf '  FAIL  %-18s (selftest)\n' "$ex"
            echo "$out" | grep "FAIL" | head -3 | sed 's/^/          /'
            fail=$((fail + 1))
        else
            printf '  PASS  %-18s (example selftest)\n' "$ex"
            pass=$((pass + 1))
        fi
    fi
done

echo ""
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
