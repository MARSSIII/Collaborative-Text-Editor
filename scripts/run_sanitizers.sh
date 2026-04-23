#!/usr/bin/env bash
set -u -o pipefail

# Usage:
#   scripts/run_sanitizers.sh              # run all three: asan, ubsan, tsan
#   scripts/run_sanitizers.sh asan tsan    # run only the listed sanitizers

cd "$(dirname "$0")/.."

SANITIZERS=("$@")
if [ ${#SANITIZERS[@]} -eq 0 ]; then
    SANITIZERS=(address undefined thread)
fi

alias_of() {
    case "$1" in
        asan|address)    echo "address" ;;
        ubsan|undefined) echo "undefined" ;;
        tsan|thread)     echo "thread" ;;
        *) echo "" ;;
    esac
}

dir_of() {
    case "$1" in
        address)   echo "build-asan" ;;
        undefined) echo "build-ubsan" ;;
        thread)    echo "build-tsan" ;;
    esac
}

declare -a PASSED=()
declare -a FAILED=()
OVERALL=0

run_one() {
    local san="$1"
    local dir
    dir=$(dir_of "$san")

    echo
    echo ">>> $san ($dir)"

    if ! cmake -B "$dir" -S . -DSANITIZE="$san" >/dev/null; then
        FAILED+=("$san (configure)"); OVERALL=1; return
    fi
    if ! cmake --build "$dir" -j >/dev/null; then
        FAILED+=("$san (build)"); OVERALL=1; return
    fi
    local env_prefix=""
    if [ "$san" = "thread" ]; then
        env_prefix="TSAN_OPTIONS=suppressions=$(pwd)/scripts/tsan_suppressions.txt"
    fi

    if (cd "$dir" && env $env_prefix ctest --output-on-failure); then
        PASSED+=("$san")
    else
        FAILED+=("$san (tests)"); OVERALL=1
    fi
}

for raw in "${SANITIZERS[@]}"; do
    san=$(alias_of "$raw")
    if [ -z "$san" ]; then
        echo "unknown sanitizer: $raw (expected asan|ubsan|tsan)" >&2
        OVERALL=1
        continue
    fi
    run_one "$san"
done

echo
for s in "${PASSED[@]}"; do echo "PASS $s"; done
for s in "${FAILED[@]}"; do echo "FAIL $s"; done
exit "$OVERALL"
