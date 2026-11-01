#!/bin/bash
# Download and extract .deb packages into a local prefix (no root needed).
# Usage: fetch_debs.sh <prefix> <pkg1> [pkg2 ...]
set -e
PREFIX="$1"; shift
ROOT="$PREFIX/root"
mkdir -p "$PREFIX/debs" "$ROOT"
cd "$PREFIX/debs"
echo ">> Downloading: $*"
# apt-get download accepts multiple packages at once
apt-get download "$@" 2>&1 | tail -5 || true
echo ">> Extracting debs into $ROOT"
for d in *.deb; do
  [ -e "$d" ] || continue
  dpkg-deb -x "$d" "$ROOT" 2>/dev/null || echo "  (failed $d)"
done
echo ">> Done. Root at $ROOT"
find "$ROOT/usr/bin" -maxdepth 1 -type f 2>/dev/null | head -40
