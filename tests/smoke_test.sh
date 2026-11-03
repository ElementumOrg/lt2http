#!/bin/bash
# End-to-end test of a built lt2http binary, needs no network: tests/smoke_test.sh <lt2http binary>
# KEEP_WORK=1 keeps the temporary directory with the logs.
#
# 1. API basics on a magnet without peers.
# 2. File storage: a torrent seeded from a local file, streaming (full, ranged, HEAD, 416), /generate.
# 3. A restart reloads the torrent with its resume data.
# 4. Memory storage: a second lt2http downloads from the first over localhost and streams, and the
#    torrent is removed while a slow stream is still reading.
set -u

BIN=$(readlink -f "$1")
TESTS=$(dirname "$(readlink -f "$0")")
WORK=$(mktemp -d)
SEEDER_PORT=65291
DOWNLOADER_PORT=65292
PEER_PORT=65293
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
    "$BIN" --config="$WORK/$name/config.json" --web_port="$port" --torrents_path="$WORK/$name/torrents" \
        --disable_dht=true --disable_lsd=true --disable_upnp=true "$@" >> "$WORK/$name.log" 2>&1 &
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

# Test data: 3 MB file with 16 KB pieces, so a read crosses piece boundaries
mkdir -p "$WORK/seeder/downloads"
head -c 3000000 /dev/urandom > "$WORK/seeder/downloads/movie.mp4"
HASH=$(python3 -I "$TESTS/torrent.py" create "$WORK/seeder/downloads/movie.mp4" "$WORK/test.torrent" 16384)
MOVIE="$WORK/seeder/downloads/movie.mp4"

echo "== API basics"
start seeder $SEEDER_PORT --download_path="$WORK/seeder/downloads" --download_storage=file \
    --listen_auto_detect_ip=false --listen_interfaces=127.0.0.1 --listen_auto_detect_port=false \
    --listen_port_min=$PEER_PORT --listen_port_max=$PEER_PORT || exit 1
U=http://localhost:$SEEDER_PORT
MAGNET_HASH=dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c
check "status" contains "$(get $U/status)" '"torrents_count":0'
check "add magnet" contains "$(get "$U/service/add/uri?uri=magnet%3A%3Fxt%3Durn%3Abtih%3A$MAGNET_HASH%26dn%3DTest&storage=memory")" '"success":true'
check "magnet name before metadata" contains "$(get $U/torrents)" '"name":"Test"'
check "upper case hash" contains "$(get "$U/torrents/${MAGNET_HASH^^}/status")" '"state_string":"Downloading metadata"'
check "unknown torrent" contains "$(get $U/torrents/nope/info)" "Torrent with hash 'nope' not found"
check "no metadata" contains "$(get $U/torrents/$MAGNET_HASH/generate)" '"error":"Torrent does not have metadata"'
check "bad storage" contains "$(get "$U/service/add/uri?uri=x&storage=bogus")" '"success":false'
check "remove magnet" contains "$(get $U/torrents/$MAGNET_HASH/remove)" '"success":true'
check "changelog" equals "$(get $U/changelog)" "$(cat "$TESTS/../changelog.md")"

echo "== File storage"
check "add torrent file" contains "$(curl -s -m 10 -F "file=@$WORK/test.torrent" "$U/service/add/file?storage=file")" "\"hash\":\"$HASH\""
check "files" contains "$(get "$U/torrents/$HASH/files?status=true")" '"name":"movie.mp4"'
# Files are added deselected, selecting one checks the existing data
check "buffer" contains "$(get "$U/torrents/$HASH/files/0/download?buffer=true")" '"success":true'
check "buffering_total" contains "$(get "$U/torrents/$HASH/files/0/status")" '"buffering_total":3000000'
wait_for "$U/torrents/$HASH/files/0/status" '"total_done":3000000' 30
check "seeded file is complete" contains "$(get "$U/torrents/$HASH/files/0/status")" '"total_done":3000000'
get -o "$WORK/generated.torrent" "$U/torrents/$HASH/generate"
check "generate" equals "$(python3 -I "$TESTS/torrent.py" hash "$WORK/generated.torrent")" "$HASH"
STREAM=$U/torrents/$HASH/files/0/stream/movie.mp4
get -o "$WORK/stream.bin" "$STREAM"
check "full stream" cmp -s "$WORK/stream.bin" "$MOVIE"
get -H "Range: bytes=1000000-2100000" -o "$WORK/range.bin" "$STREAM"
check "range stream" cmp -s "$WORK/range.bin" <(tail -c +1000001 "$MOVIE" | head -c 1100001)
check "range past the end" equals "$(get -o /dev/null -w '%{http_code}' -H "Range: bytes=5000000-" "$STREAM")" "416"
check "head" contains "$(get -I "$STREAM" | tr -d '\r')" "Content-Length: 3000000"
check "info page" contains "$(get $U/info)" "movie.mp4"
check "seeder stops" stop seeder $SEEDER_PORT

echo "== Restart"
start seeder $SEEDER_PORT --download_path="$WORK/seeder/downloads" --download_storage=file --autoload_torrents_paused=false \
    --listen_auto_detect_ip=false --listen_interfaces=127.0.0.1 --listen_auto_detect_port=false \
    --listen_port_min=$PEER_PORT --listen_port_max=$PEER_PORT || exit 1
wait_for "$U/torrents/$HASH/status" '"total_done":3000000' 30
check "reloaded with resume data" contains "$(cat "$WORK/seeder.log")" "Using resume data"
check "reloaded torrent is complete" contains "$(get "$U/torrents/$HASH/status")" '"total_done":3000000'

echo "== Memory storage from a local peer"
start downloader $DOWNLOADER_PORT --download_path="$WORK/downloader" --download_storage=memory \
    --listen_auto_detect_ip=false --listen_interfaces=127.0.0.1 || exit 1
D=http://localhost:$DOWNLOADER_PORT
check "add magnet with peer" contains "$(get "$D/service/add/uri?uri=magnet%3A%3Fxt%3Durn%3Abtih%3A$HASH%26x.pe%3D127.0.0.1%3A$PEER_PORT")" '"success":true'
wait_for "$D/torrents/$HASH/info" '"has_metadata":true' 30
check "metadata from peer" contains "$(get "$D/torrents/$HASH/info")" '"has_metadata":true'
STREAM=$D/torrents/$HASH/files/0/stream/movie.mp4
get -m 60 -o "$WORK/memory.bin" "$STREAM"
check "memory stream" cmp -s "$WORK/memory.bin" "$MOVIE"
# A slow client keeps the reader busy while the torrent is removed
curl -s -m 30 --limit-rate 100K -o /dev/null "$STREAM" &
SLOW=$!
sleep 2
check "remove while streaming" contains "$(get "$D/torrents/$HASH/remove")" '"success":true'
wait $SLOW
check "alive after remove" contains "$(get "$D/status")" '"torrents_count":0'
check "downloader stops" stop downloader $DOWNLOADER_PORT
check "seeder stops after restart" stop seeder $SEEDER_PORT

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
