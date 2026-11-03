#!/usr/bin/env python3
"""Test helper for torrent files.

torrent.py create <data_file> <out.torrent> <piece_length>   prints the info hash
torrent.py hash <file.torrent>                               prints the info hash
"""
import hashlib
import os
import sys


def bencode(value):
    if isinstance(value, int):
        return b"i%de" % value
    if isinstance(value, str):
        value = value.encode()
    if isinstance(value, bytes):
        return b"%d:%s" % (len(value), value)
    if isinstance(value, list):
        return b"l" + b"".join(map(bencode, value)) + b"e"
    return b"d" + b"".join(bencode(k) + bencode(v) for k, v in sorted(value.items())) + b"e"


def bdecode(data, i=0):
    """Returns (value, next index), dictionary keys stay bytes."""
    c = data[i:i + 1]
    if c == b"i":
        end = data.index(b"e", i)
        return int(data[i + 1:end]), end + 1
    if c in (b"l", b"d"):
        items, i = [], i + 1
        while data[i:i + 1] != b"e":
            item, i = bdecode(data, i)
            items.append(item)
        return (items if c == b"l" else dict(zip(items[::2], items[1::2]))), i + 1
    colon = data.index(b":", i)
    end = colon + 1 + int(data[i:colon])
    return data[colon + 1:end], end


def create(data_file, out, piece_length):
    raw = open(data_file, "rb").read()
    pieces = b"".join(hashlib.sha1(raw[i:i + piece_length]).digest() for i in range(0, len(raw), piece_length))
    info = {"name": os.path.basename(data_file), "length": len(raw), "piece length": piece_length, "pieces": pieces}
    open(out, "wb").write(bencode({"announce": "udp://127.0.0.1:1/announce", "info": info}))
    return hashlib.sha1(bencode(info)).hexdigest()


def info_hash(torrent_file):
    torrent, _ = bdecode(open(torrent_file, "rb").read())
    info = {k.decode(): v for k, v in torrent[b"info"].items()}
    return hashlib.sha1(bencode(info)).hexdigest()


if __name__ == "__main__":
    if sys.argv[1] == "create":
        print(create(sys.argv[2], sys.argv[3], int(sys.argv[4])))
    else:
        print(info_hash(sys.argv[2]))
