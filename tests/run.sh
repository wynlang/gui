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
#
# HOST EXHAUSTION IS RETRIED; A REAL FAILURE NEVER IS.
#
# Several suites here shell out to `wyn build` (test_codegen and test_pipeline
# generate .wyn files and compile them; test_designer builds the designer itself).
# Under load - a parallel agent, a `make -j`, another suite - those child builds can
# fail to fork rather than fail to compile, and this gate reported that as a test
# failure. Measured at load average 12: roughly one suite in four, and a DIFFERENT
# suite each time, which is the signature of contention rather than a defect.
#
# So retry ONLY the transient shapes, matching the list tests/run_bdd.sh already uses
# in the compiler repo. A genuine assertion failure or compiler diagnostic is
# returned on the first attempt, unretried - masking one of those would be far worse
# than a flaky gate.
run_one() {
    local attempt=0 out
    while [ "$attempt" -lt 3 ]; do
        out=$(perl -e 'alarm(shift @ARGV); exec @ARGV' 300 "$WYN" run "$1" 2>&1)
        if echo "$out" | grep -qiE 'Resource temporarily unavailable|posix_spawn|unable to fork|Cannot allocate memory|too many open files'; then
            attempt=$((attempt + 1))
            sleep "0.$((attempt * 3))"
            continue
        fi
        break
    done
    printf '%s' "$out"
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
        # A ZERO assertion count is a FAILURE, not a pass. A test that needs a window
        # and does not get one skips its own body and still prints its "ALL ... PASSED"
        # banner, so counting the banner alone reported silent non-coverage as success.
        # That is how this gate could have gone green while testing nothing.
        if [ "$n" -eq 0 ]; then
            printf '  FAIL  %-18s (passed banner but ZERO assertions - skipped itself?)\n' "$name"
            fail=$((fail + 1))
        else
            printf '  PASS  %-18s (%s assertions)\n' "$name" "$n"
            pass=$((pass + 1))
        fi
    elif echo "$out" | grep -qE "^.*[0-9]+ tests passed"; then
        n=$(echo "$out" | grep -oE "[0-9]+ tests passed" | head -1)
        ncount=${n%% *}
        if [ "$ncount" -eq 0 ]; then
            printf '  FAIL  %-18s (0 tests passed - skipped itself?)\n' "$name"
            fail=$((fail + 1))
        else
            printf '  PASS  %-18s (%s)\n' "$name" "$n"
            pass=$((pass + 1))
        fi
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
        # Count CHECKED STEPS. The two selftests report differently - wizard prints
        # "  ok   ..." lines, designer narrates each step and marks its assertions
        # "(expect N)" - so the shared evidence is the "-- section --" markers plus
        # either form of assertion. Any of them proves the body ran.
        ok_n=$(echo "$out" | grep -cE "^  ok |\(expect |^-- " || true)
        if echo "$out" | grep -q "FAIL"; then
            printf '  FAIL  %-18s (selftest)\n' "$ex"
            echo "$out" | grep "FAIL" | head -3 | sed 's/^/          /'
            fail=$((fail + 1))
        elif [ "$ok_n" -eq 0 ]; then
            # Absence of "FAIL" used to be the WHOLE pass condition, so a selftest that
            # printed NOTHING - no window, an early return, a silent crash under the
            # alarm - was recorded as a pass. Require positive evidence instead.
            printf '  FAIL  %-18s (selftest produced no checked steps)\n' "$ex"
            fail=$((fail + 1))
        else
            printf '  PASS  %-18s (example selftest, %s checked steps)\n' "$ex" "$ok_n"
            pass=$((pass + 1))
        fi
    fi
done

echo ""
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
