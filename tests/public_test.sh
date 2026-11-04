#!/bin/bash
# End-to-end test with a public, legal torrent from the internet: tests/public_test.sh <lt2http binary>
# Big Buck Bunny (CC BY 3.0) from webtorrent.io, its web seed gives the data over HTTP when no peers are around.
# Downloads about 10 MB. KEEP_WORK=1 keeps the temporary directory with the logs.
#
# 1. Add the torrent by URL, list it and its files.
# 2. Meta info: set, read, edit, see it in the torrent list.
# 3. Download selected small files, stream them, compare with the files on disk.
# 4. Buffer and stream parts of the big file, players.
# 5. Stop files, pause and resume.
# 6. A restart keeps the torrent, its files and meta info.
# 7. Remove the torrent with its data.
# 8. Memory storage: add again and stream from memory.

# shellcheck source=common.sh
source "$(dirname "$0")/common.sh"

PORT=65295
U=http://localhost:$PORT
TORRENT_URL=https://webtorrent.io/torrents/big-buck-bunny.torrent
HASH=dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c
# File indexes and sizes in the torrent
SRT=0
SRT_SIZE=140
MP4=1
POSTER=2
POSTER_SIZE=310380
T=$U/torrents/$HASH
DOWNLOADS=$WORK/client/downloads
# DHT stays on, it may find peers
ARGS=(--download_path="$DOWNLOADS" --autoload_torrents_paused=false --disable_upnp=true --disable_lsd=true)

if ! curl -sf -m 30 -o "$WORK/bbb.torrent" "$TORRENT_URL"; then
    echo "Cannot download $TORRENT_URL, this test needs internet access"
    exit 2
fi

add() { # add <storage>: adds the torrent by its URL, with a header for the request
    curl -s -m 60 --get --data-urlencode "uri=$TORRENT_URL|User-Agent=lt2http-test" --data "storage=$1" "$U/service/add/uri"
}

echo "== Add"
check "torrent hash" equals "$(python3 -I "$TESTS/torrent.py" hash "$WORK/bbb.torrent")" "$HASH"
start client $PORT --download_storage=file "${ARGS[@]}" || exit 1
check "status" contains "$(get $U/status)" '"torrents_count":0'
ADDED=$(add file)
check "add by url" contains "$ADDED" "\"hash\":\"$HASH\""
check "added with metadata" contains "$ADDED" '"has_metadata":true'
check "listed" contains "$(get $U/torrents)" '"name":"Big Buck Bunny"'
check "info" contains "$(get "$T/info")" '"total_files":3,"chosen_files":0'
FILES=$(get "$T/files")
check "files" contains "$FILES" '"name":"Big Buck Bunny.mp4"'
check "file size" contains "$FILES" "\"size\":$POSTER_SIZE"
check "info page" contains "$(get "$U/info?torrent=$HASH")" "poster.jpg"
check "trackers" contains "$(get "$T/trackers")" '"url":"udp://tracker.leechers-paradise.org:6969","tier":0'
check "pieces" contains "$(get "$T/pieces")" '"total":1055'
check "download path of a folder" contains "$(get "$T/info")" "\"download_path\":\"$DOWNLOADS/Big Buck Bunny\""

echo "== Meta info"
check "no meta info" equals "$(get "$T/meta_info")" "null"
check "set meta info" contains "$(get -d '{"tmdb_id": 10378, "title": "Big Buck Bunny", "year": 2008}' "$T/meta_info")" '"success":true'
check "read meta info" equals "$(get "$T/meta_info")" '{"tmdb_id":10378,"title":"Big Buck Bunny","year":2008}'
check "edit meta info" contains "$(get -d '{"tmdb_id": 10378, "title": "Big Buck Bunny", "watched": true}' "$T/meta_info")" '"success":true'
check "read edited meta info" equals "$(get "$T/meta_info")" '{"tmdb_id":10378,"title":"Big Buck Bunny","watched":true}'
check "meta info in list" contains "$(get $U/torrents)" '"meta_info":{"tmdb_id":10378,"title":"Big Buck Bunny","watched":true}'

echo "== Download files"
check "select poster" contains "$(get "$T/files/$POSTER/download")" '"success":true'
check "select subtitles" contains "$(get "$T/files/$SRT/download")" '"success":true'
wait_for "$T/files/$POSTER/status" "\"total_done\":$POSTER_SIZE" 180
check "poster downloaded" contains "$(get "$T/files/$POSTER/status")" "\"total_done\":$POSTER_SIZE"
wait_for "$T/files/$SRT/status" "\"total_done\":$SRT_SIZE" 60
check "subtitles downloaded" contains "$(get "$T/files/$SRT/status")" "\"total_done\":$SRT_SIZE"
check "chosen files" contains "$(get "$T/info")" '"chosen_files":2'
get -o "$WORK/poster.jpg" "$T/files/$POSTER/stream/poster.jpg"
check "stream poster" equals "$(head -c 3 "$WORK/poster.jpg" | od -An -tx1 | tr -d ' ')" "ffd8ff"
check "poster on disk" cmp -s "$WORK/poster.jpg" "$DOWNLOADS/Big Buck Bunny/poster.jpg"
check "stream subtitles" contains "$(get "$T/files/$SRT/stream/subtitles.srt")" "-->"

echo "== Buffer and stream the movie"
check "buffer at 100 MB" contains "$(get "$T/files/$MP4/buffer?position=100000000&length=2000000")" '"success":true'
wait_for "$T/files/$MP4/status" '"buffering_progress":100' 300
check "buffered" contains "$(get "$T/files/$MP4/status")" '"buffering_progress":100'
MOVIE=$T/files/$MP4/stream/movie.mp4
curl -s -m 300 -H "Range: bytes=0-65535" -o "$WORK/start.mp4" "$MOVIE"
check "stream movie start" equals "$(head -c 8 "$WORK/start.mp4" | tail -c 4)" "ftyp"
curl -s -m 300 -H "Range: bytes=100000000-100999999" -o "$WORK/middle.mp4" "$MOVIE"
check "stream buffered part" equals "$(stat -c %s "$WORK/middle.mp4")" "1000000"
check "head" contains "$(get -I "$MOVIE" | tr -d '\r')" "Content-Length: 276134947"
check "add player" contains "$(get "$T/players/add")" '"success":true'
check "playing" contains "$(get "$T/status")" '"state_string":"Playing"'
check "remove player" contains "$(get "$T/players/remove")" '"success":true'

echo "== Stop files, pause"
check "stop movie" contains "$(get "$T/files/$MP4/stop")" '"success":true'
check "stop subtitles" contains "$(get "$T/files/$SRT/set_priority?priority=0")" '"success":true'
# libtorrent applies file priorities in the disk thread, a moment later
wait_for "$T/info" '"chosen_files":1' 10
check "only poster chosen" contains "$(get "$T/info")" '"chosen_files":1'
check "pause" contains "$(get "$T/pause")" '"success":true'
check "paused" contains "$(get "$T/info?status=true")" '"is_paused":true'
check "paused state" contains "$(get "$T/status")" '"state_string":"Paused"'
check "resume" contains "$(get "$T/resume")" '"success":true'
check "resumed" contains "$(get "$T/info")" '"is_paused":false'
check "client stops" stop client $PORT

echo "== Restart"
start client $PORT --download_storage=file "${ARGS[@]}" || exit 1
wait_for "$T/files/$POSTER/status" "\"total_done\":$POSTER_SIZE" 60
check "reloaded torrent" contains "$(get "$T/files/$POSTER/status")" "\"total_done\":$POSTER_SIZE"
check "reloaded meta info" equals "$(get "$T/meta_info")" '{"tmdb_id":10378,"title":"Big Buck Bunny","watched":true}'
check "reloaded priorities" contains "$(get "$T/info")" '"chosen_files":1'
check "clear meta info" contains "$(get -d 'null' "$T/meta_info")" '"success":true'
check "meta info cleared" equals "$(get "$T/meta_info")" "null"

echo "== Remove"
check "remove with data" contains "$(get "$T/remove?deleteData=true")" '"success":true'
check "removed" contains "$(get $U/status)" '"torrents_count":0'
for _ in $(seq 1 40); do [ -e "$DOWNLOADS/Big Buck Bunny/poster.jpg" ] || break; sleep 0.25; done
check "data removed" test ! -e "$DOWNLOADS/Big Buck Bunny/poster.jpg"
check "torrent files removed" bash -c "! ls '$WORK/client/torrents/$HASH'.* 2>/dev/null"
check "client stops after remove" stop client $PORT

echo "== Memory storage"
start client $PORT --download_storage=memory "${ARGS[@]}" || exit 1
check "add to memory" contains "$(add memory)" "\"hash\":\"$HASH\""
check "memory storage" contains "$(get "$T/info")" '"is_memory_storage":true'
curl -s -m 300 -o "$WORK/memory.jpg" "$T/files/$POSTER/stream/poster.jpg"
check "stream from memory" cmp -s "$WORK/memory.jpg" "$WORK/poster.jpg"
check "remove from memory" contains "$(get "$T/remove")" '"success":true'
check "client stops after memory" stop client $PORT

finish
