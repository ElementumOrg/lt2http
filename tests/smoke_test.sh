#!/bin/bash
# End-to-end test of a built lt2http binary, needs no network: tests/smoke_test.sh <lt2http binary>
# KEEP_WORK=1 keeps the temporary directory with the logs. tests/public_test.sh tests with a torrent from the internet.
#
# 1. API basics on a magnet without peers.
# 2. File storage: a torrent seeded from a local file, streaming (full, ranged, HEAD, 416), /generate.
# 3. A restart reloads the torrent with its resume data.
# 4. Memory storage: a second lt2http downloads from the first over localhost and streams, and the
#    torrent is removed while a slow stream is still reading.
# shellcheck source=common.sh
source "$(dirname "$0")/common.sh"

SEEDER_PORT=65291
DOWNLOADER_PORT=65292
PEER_PORT=65293
# No DHT, LSD and UPnP: the test runs without network
OFFLINE=(--disable_dht=true --disable_lsd=true --disable_upnp=true)

# Test data: 3 MB file with 16 KB pieces, so a read crosses piece boundaries
mkdir -p "$WORK/seeder/downloads"
head -c 3000000 /dev/urandom > "$WORK/seeder/downloads/movie.mp4"
HASH=$(python3 -I "$TESTS/torrent.py" create "$WORK/seeder/downloads/movie.mp4" "$WORK/test.torrent" 16384)
MOVIE="$WORK/seeder/downloads/movie.mp4"

echo "== API basics"
start seeder $SEEDER_PORT --download_path="$WORK/seeder/downloads" --download_storage=file "${OFFLINE[@]}" --player_timeout=3 --proxy_password=hidden1 \
    --listen_auto_detect_ip=false --listen_interfaces=127.0.0.1 --listen_auto_detect_port=false \
    --listen_port_min=$PEER_PORT --listen_port_max=$PEER_PORT || exit 1
U=http://localhost:$SEEDER_PORT
MAGNET_HASH=dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c
check "status" contains "$(get $U/status)" '"torrents_count":0'
check "status versions" contains "$(get $U/status)" '"libtorrent_version":"1.2.'
check "status commit" contains "$(get $U/status)" '"commit":"'
check "add magnet" contains "$(get "$U/service/add/uri?uri=magnet%3A%3Fxt%3Durn%3Abtih%3A$MAGNET_HASH%26dn%3DTest&storage=memory")" '"success":true'
check "magnet name before metadata" contains "$(get $U/torrents)" '"name":"Test"'
check "upper case hash" contains "$(get "$U/torrents/${MAGNET_HASH^^}/status")" '"state_string":"Finding"'
check "localized state" contains "$(get "$U/torrents/$MAGNET_HASH/status?lang=de")" '"state_string":"Finding","state_localized":"Suchen"'
check "default language state" contains "$(get "$U/torrents/$MAGNET_HASH/status")" '"state_localized":"Finding"'
check "localized state in list" contains "$(get "$U/torrents?status=true&lang=ru-RU")" '"state_localized":"Поиск"'
check "localized /info" contains "$(get "$U/info?lang=fr")" 'Supprimer'
check "trackers without metadata" equals "$(get "$U/torrents/$MAGNET_HASH/trackers")" '[]'
check "pieces without metadata" contains "$(get "$U/torrents/$MAGNET_HASH/pieces")" '"total":0'
check "no meta info" equals "$(get $U/torrents/$MAGNET_HASH/meta_info)" "null"
check "set meta info" contains "$(get -d '{"tmdb": 123, "title": "Test <b>", "tags": ["a", 1.5, true, null]}' $U/torrents/$MAGNET_HASH/meta_info)" '"success":true'
check "get meta info" equals "$(get $U/torrents/$MAGNET_HASH/meta_info)" '{"tmdb":123,"title":"Test <b>","tags":["a",1.5,true,null]}'
check "meta info in list" contains "$(get $U/torrents)" '"meta_info":{"tmdb":123,'
check "meta info escaped in /info" contains "$(get $U/info)" 'Test &lt;b&gt;'
check "invalid meta info" contains "$(get -d '{"tmdb":' $U/torrents/$MAGNET_HASH/meta_info)" '"success":false'
check "meta info kept after invalid" contains "$(get $U/torrents/$MAGNET_HASH/meta_info)" '"tmdb":123'
check "remove meta info" contains "$(get -d 'null' $U/torrents/$MAGNET_HASH/meta_info)" '"success":true'
check "meta info removed" equals "$(get $U/torrents/$MAGNET_HASH/meta_info)" "null"
check "add player" contains "$(get $U/torrents/$MAGNET_HASH/players/add)" '"success":true'
check "players" contains "$(get $U/torrents/$MAGNET_HASH/info)" '"players":1'
get $U/torrents/$MAGNET_HASH/players/remove > /dev/null
check "players never below 0" contains "$(get $U/torrents/$MAGNET_HASH/players/remove)" '"success":true'
check "no players" contains "$(get $U/torrents/$MAGNET_HASH/info)" '"players":0'
get $U/torrents/$MAGNET_HASH/players/add > /dev/null
wait_for "$U/torrents/$MAGNET_HASH/info" '"players":0' 10
check "player without streams expires" contains "$(get $U/torrents/$MAGNET_HASH/info)" '"players":0'
check "no file priorities in memory storage" contains "$(get $U/torrents/$MAGNET_HASH/files/stop)" 'not used with memory storage'
check "unknown torrent" contains "$(get $U/torrents/nope/info)" "Torrent with hash 'nope' not found"
check "no metadata" contains "$(get $U/torrents/$MAGNET_HASH/generate)" '"error":"Torrent does not have metadata"'
check "header not key=value" contains "$(get --get --data-urlencode "uri=http://127.0.0.1:$SEEDER_PORT/changelog|nope" "$U/service/add/uri")" "is not in key=value format"
check "link is not a torrent" contains "$(get --get --data-urlencode "uri=http://127.0.0.1:$SEEDER_PORT/changelog|User-Agent=test" "$U/service/add/uri")" 'Not a torrent file'
echo "not a torrent" > "$WORK/bad.torrent"
check "upload is not a torrent" contains "$(curl -s -m 10 -F "file=@$WORK/bad.torrent" "$U/service/add/file")" 'Not a torrent file'
check "web ui" contains "$(get $U/web/)" 'src="/web/assets/index.js"'
check "web ui script" equals "$(get -o /dev/null -w '%{http_code} %{content_type}' $U/web/assets/index.js)" "200 text/javascript; charset=utf-8"
check "web ui style" equals "$(get -o /dev/null -w '%{http_code} %{content_type}' $U/web/assets/index.css)" "200 text/css; charset=utf-8"
check "web ui translation" contains "$(get $U/web/i18n/de_de.json)" '"_name": "Deutsch"'
check "web ui missing file" equals "$(get -o /dev/null -w '%{http_code}' $U/web/nope.js)" "404"
check "root redirects to web ui" equals "$(get -o /dev/null -w '%{redirect_url}' $U/)" "$U/web/"
check "languages" contains "$(get $U/languages)" '{"code":"uk_ua","name":"Українська"}'
STATE=$(get $U/settings/state)
check "settings state" contains "$STATE" '"listen_port":'
check "libtorrent settings" contains "$STATE" '"user_agent":'
check "password hidden" contains "$STATE" '"proxy_password":"***"'
check "password not in state" bash -c "! grep -q hidden1 <<< '$STATE'"
check "password not logged" bash -c "! grep -q hidden1 '$WORK/seeder.log'"
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
check "trackers" contains "$(get "$U/torrents/$HASH/trackers")" '"url":"udp://127.0.0.1:1/announce","tier":0'
check "download path of a single file" contains "$(get "$U/torrents/$HASH/info")" "\"download_path\":\"$WORK/seeder/downloads\""
check "piece length" contains "$(get "$U/torrents/$HASH/info")" '"piece_length":16384'
check "info page of one torrent" contains "$(get "$U/info?torrent=$HASH&pieces=false")" "Only this torrent"
check "info page without pieces" bash -c "! curl -s '$U/info?pieces=false' | grep -q 'Pieces ('"
wait_for "$U/torrents/$HASH/status" '"state_string":"Seeding"' 10
check "seeding" contains "$(get "$U/torrents/$HASH/status")" '"state_string":"Seeding"'
check "pieces" contains "$(get "$U/torrents/$HASH/pieces")" '"total":184,"stored":184'
get "$U/torrents/$HASH/players/add" > /dev/null
wait_for "$U/torrents/$HASH/status" '"state_string":"Playing"' 5
check "playing" contains "$(get "$U/torrents/$HASH/status")" '"state_string":"Playing"'
get "$U/torrents/$HASH/players/remove" > /dev/null
check "stop all files" contains "$(get "$U/torrents/$HASH/files/stop")" '"success":true'
# libtorrent applies file priorities in the disk thread, a moment later
wait_for "$U/torrents/$HASH/info" '"chosen_files":0' 5
check "chosen files after stop" contains "$(get "$U/torrents/$HASH/info")" '"chosen_files":0'
check "set priority" contains "$(get "$U/torrents/$HASH/files/0/set_priority?priority=7")" '"success":true'
wait_for "$U/torrents/$HASH/files/0/status" '"priority":7' 5
check "priority set" contains "$(get "$U/torrents/$HASH/files/0/info?status=true")" '"priority":7'
check "bad priority" contains "$(get "$U/torrents/$HASH/files/0/set_priority?priority=9")" 'Priority must be from 0 to 7'
check "download all files" contains "$(get "$U/torrents/$HASH/files/download")" '"success":true'
wait_for "$U/torrents/$HASH/files/0/status" '"priority":1' 5
check "priority after download all" contains "$(get "$U/torrents/$HASH/files/0/status")" '"priority":1'
# 16 KB pieces from the one with byte 1000000 to the end, as the end buffer reaches the position
check "buffer at position" contains "$(get "$U/torrents/$HASH/files/0/buffer?position=1000000&length=100000")" '"success":true'
check "buffer at position size" contains "$(get "$U/torrents/$HASH/files/0/status")" '"buffering_total":2000576'
check "set file meta info" contains "$(get -d '{"kept": true}' "$U/torrents/$HASH/meta_info")" '"success":true'
check "api docs" contains "$(get $U/api-docs/oas-3.0.0.json)" '"meta_info"'
check "seeder stops" stop seeder $SEEDER_PORT

echo "== Restart"
start seeder $SEEDER_PORT --download_path="$WORK/seeder/downloads" --download_storage=file "${OFFLINE[@]}" --autoload_torrents_paused=false \
    --listen_auto_detect_ip=false --listen_interfaces=127.0.0.1 --listen_auto_detect_port=false \
    --listen_port_min=$PEER_PORT --listen_port_max=$PEER_PORT || exit 1
wait_for "$U/torrents/$HASH/status" '"total_done":3000000' 30
check "reloaded with resume data" contains "$(cat "$WORK/seeder.log")" "Using resume data"
check "reloaded torrent is complete" contains "$(get "$U/torrents/$HASH/status")" '"total_done":3000000'
check "reloaded meta info" equals "$(get "$U/torrents/$HASH/meta_info")" '{"kept":true}'

echo "== Memory storage from a local peer"
start downloader $DOWNLOADER_PORT --download_path="$WORK/downloader" --download_storage=memory "${OFFLINE[@]}" --player_timeout=2 \
    --listen_auto_detect_ip=false --listen_interfaces=127.0.0.1 || exit 1
D=http://localhost:$DOWNLOADER_PORT
check "add magnet with peer" contains "$(get "$D/service/add/uri?uri=magnet%3A%3Fxt%3Durn%3Abtih%3A$HASH%26x.pe%3D127.0.0.1%3A$PEER_PORT")" '"success":true'
wait_for "$D/torrents/$HASH/info" '"has_metadata":true' 30
check "metadata from peer" contains "$(get "$D/torrents/$HASH/info")" '"has_metadata":true'
check "no download path in memory" contains "$(get "$D/torrents/$HASH/info")" '"download_path":null'
STREAM=$D/torrents/$HASH/files/0/stream/movie.mp4
get -m 60 -o "$WORK/memory.bin" "$STREAM"
check "memory stream" cmp -s "$WORK/memory.bin" "$MOVIE"
# A slow client keeps the reader busy while the torrent is removed
curl -s -m 30 --limit-rate 100K -o /dev/null "$STREAM" &
SLOW=$!
get "$D/torrents/$HASH/players/add" > /dev/null
sleep 4
check "player with a stream is kept" contains "$(get "$D/torrents/$HASH/info")" '"players":1'
check "remove while streaming" contains "$(get "$D/torrents/$HASH/remove")" '"success":true'
wait $SLOW
check "alive after remove" contains "$(get "$D/status")" '"torrents_count":0'
check "downloader stops" stop downloader $DOWNLOADER_PORT
check "seeder stops after restart" stop seeder $SEEDER_PORT

finish
