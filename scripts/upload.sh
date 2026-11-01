#!/bin/bash
# Upload a file to tmpfiles.org and print the DIRECT download URL.
# tmpfiles returns a viewer page; we extract the real /dl/.../<id>/<name> link.
set -e
F="$1"
[ -z "$F" ] && { echo "usage: $0 <file>"; exit 1; }
[ ! -f "$F" ] && { echo "not a file: $F"; exit 1; }

BNAME=$(basename "$F")
echo ">> uploading $BNAME ($(stat -c%s "$F") bytes)..." >&2
RESP=$(curl -sS --max-time 120 -F "file=@$F" https://tmpfiles.org/api/v1/upload)
echo ">> api response: $RESP" >&2
VIEWER=$(echo "$RESP" | python3 -c "import sys,json;print(json.load(sys.stdin)['data']['url'])")
echo ">> viewer: $VIEWER" >&2
# fetch viewer HTML and extract the real /dl/<token>/<id>/<name> href
HTML=$(curl -sSL --max-time 30 -A "Mozilla/5.0" "$VIEWER")
DIRECT=$(echo "$HTML" | grep -oiE 'href="https://tmpfiles\.org/dl/[^"]+"' | head -1 | sed 's/href="//;s/"$//')
if [ -z "$DIRECT" ]; then
    echo ">> could not extract direct url; falling back to viewer" >&2
    echo "$VIEWER"
else
    echo "$DIRECT"
fi
