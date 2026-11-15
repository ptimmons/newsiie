#!/bin/bash
# mkfeeds.sh <image.po> -- a 140K ProDOS disk named /FEEDS holding four
# corpus feeds, for the test build's disk entries. Built from build/corpus,
# which tools/getcorpus.sh fills; the feeds are not ours, so neither the
# corpus nor this disk is committed.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/tools/ac"
OUT="${1:?usage: mkfeeds.sh <image.po>}"

case "$OUT" in
    *.po) ;;
    *) echo "mkfeeds: '$OUT' is not a .po image" >&2; exit 1 ;;
esac
# AppleCommander will not create over an existing image.
if [ -f "$OUT" ]; then
    rm -f -- "$OUT"
fi
"$AC" -pro140 "$OUT" FEEDS
for f in hn ars bbc xkcd; do
    # -p, not -ptx: the bytes go on unchanged.
    "$AC" -p "$OUT" "$(echo "$f" | tr a-z A-Z).XML" TXT < "$ROOT/build/corpus/$f.xml"
done
"$AC" -l "$OUT"
