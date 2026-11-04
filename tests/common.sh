#!/bin/bash
# Helpers of the end-to-end tests, sourced with the lt2http binary as $1.
# KEEP_WORK=1 keeps the temporary directory with the logs.
set -u

if [ $# -lt 1 ] || [ ! -x "$1" ]; then
    echo "Usage: $0 <lt2http binary>"
    exit 2
fi

BIN=$(readlink -f "$1")
# shellcheck disable=SC2034 # used by the tests that source this file
TESTS=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
WORK=$(mktemp -d)
PIDS=()
FAILED=()

cleanup() {
    for pid in "${PIDS[@]}"; do kill -9 "$pid" 2>/dev/null; done
    if [ -n "${KEEP_WORK:-}" ]; then echo "Kept $WORK"; else rm -rf "$WORK"; fi
}
trap cleanup EXIT

check() { # check <name> <command...>
    local name=$1
    shift
    if "$@"; then
        echo "ok   $name"
    else
        echo "FAIL $name"
        FAILED+=("$name")
    fi
}

equals() { [ "$1" = "$2" ] || { echo "     expected '$2', got '$1'"; return 1; }; }
contains() { [[ "$1" == *"$2"* ]] || { echo "     '$2' not in: ${1:0:300}"; return 1; }; }

start() { # start <name> <web port> <args...>: starts lt2http and waits for its API
    local name=$1 port=$2
    shift 2
    mkdir -p "$WORK/$name"
    echo '{"add_extra_trackers": "none"}' > "$WORK/$name/config.json"
    "$BIN" --config="$WORK/$name/config.json" --web_port="$port" --torrents_path="$WORK/$name/torrents" "$@" >> "$WORK/$name.log" 2>&1 &
    PIDS+=($!)
    eval "${name}_PID=$!"
    for _ in $(seq 1 50); do
        curl -s -m 1 "localhost:$port/status" > /dev/null && return 0
        sleep 0.2
    done
    echo "lt2http '$name' did not start:"
    tail -20 "$WORK/$name.log"
    return 1
}

stop() { # stop <name> <web port>: requests a shutdown and expects a clean exit within 15 seconds
    local name=$1 port=$2 pid_var="${1}_PID"
    local pid=${!pid_var}
    curl -s -m 5 "localhost:$port/shutdown" > /dev/null
    for _ in $(seq 1 60); do
        kill -0 "$pid" 2>/dev/null || break
        sleep 0.25
    done
    if kill -0 "$pid" 2>/dev/null; then
        echo "     '$name' still running 15 seconds after /shutdown"
        return 1
    fi
    wait "$pid"
}

get() { curl -s -m 30 "$@"; }

wait_for() { # wait_for <url> <text> <seconds>: polls the url until the response contains the text
    for _ in $(seq 1 $(($3 * 4))); do
        get "$1" | grep -qF "$2" && return 0
        sleep 0.25
    done
    return 1
}

finish() { # checks the logs for crashes, prints them on failures and exits
    for log in "$WORK"/*.log; do
        check "no crash in $(basename "$log")" bash -c "! grep -E 'AddressSanitizer|runtime error:|LeakSanitizer|terminate called' '$log'"
    done

    if [ ${#FAILED[@]} -gt 0 ]; then
        for log in "$WORK"/*.log; do
            echo "== $(basename "$log")"
            grep -vE "portmap|Alert: " "$log" | tail -40
        done
        echo "${#FAILED[@]} failed: ${FAILED[*]}"
        exit 1
    fi
    echo "All passed"
}
