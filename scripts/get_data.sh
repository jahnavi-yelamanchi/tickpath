#!/usr/bin/env bash
# Download one official Nasdaq TotalView-ITCH 5.0 sample day.
#
#   scripts/get_data.sh [FILE]      download FILE (default below) into data/
#   scripts/get_data.sh --list      list files on the server
#
# Source listing (checked 2026-11-19): https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/
# Files are gzip, several GB. Older days ship a .md5sum; newer ones a .done.
# After download the script verifies the checksum (when published) and the
# framing of the first message: 2-byte big-endian length 12, type 'S'.
set -euo pipefail

BASE_URL="https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH"
DEFAULT_FILE="01302019.NASDAQ_ITCH50.gz" # 4.7 GB, has .md5sum
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${OUT_DIR:-$ROOT/data}"

if [[ "${1:-}" == "--list" ]]; then
    curl -fsS "$BASE_URL/" | grep -oE '[A-Za-z0-9_.-]+\.gz' | sort -u
    exit 0
fi

FILE="${1:-$DEFAULT_FILE}"
mkdir -p "$OUT_DIR"
DEST="$OUT_DIR/$FILE"

echo "downloading $BASE_URL/$FILE -> $DEST"
curl -fL --retry 5 --retry-delay 10 -C - -o "$DEST" "$BASE_URL/$FILE"

if curl -fsS -o "$DEST.md5sum" "$BASE_URL/$FILE.md5sum" 2>/dev/null; then
    expected="$(grep -oiE '[0-9a-f]{32}' "$DEST.md5sum" | head -1 | tr 'A-F' 'a-f')"
    actual="$(md5sum "$DEST" | cut -d' ' -f1)"
    if [[ "$expected" != "$actual" ]]; then
        echo "md5 mismatch: expected $expected, got $actual" >&2
        exit 1
    fi
    echo "md5 ok: $actual"
else
    rm -f "$DEST.md5sum"
    echo "no .md5sum published for $FILE; skipping checksum"
fi

first="$( { gzip -dc "$DEST" 2>/dev/null || true; } | head -c 3 | od -An -tx1 | tr -d ' \n')"
if [[ "$first" != "000c53" ]]; then
    echo "framing check failed: first bytes $first, expected 000c53 (len 12, 'S')" >&2
    exit 1
fi
echo "framing ok: first frame is a 12-byte System Event"

{
    echo "file=$FILE"
    echo "url=$BASE_URL/$FILE"
    echo "bytes=$(stat -c %s "$DEST")"
    echo "md5=$(md5sum "$DEST" | cut -d' ' -f1)"
    echo "downloaded=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$DEST.provenance"
echo "wrote $DEST.provenance"
