#!/bin/bash
# getcorpus.sh -- download real feeds into build/corpus for `make corpus`.
#
# The feeds are other people's writing, so they are fetched fresh rather
# than committed. They change daily: the corpus test checks properties
# (every item found, every link exact, only printable ASCII out) rather
# than exact text. The hand-written fixtures in tests/fixtures are the
# exact-output tests.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/corpus"
mkdir -p "$OUT"

FEEDS=(
    "hn        https://hnrss.org/frontpage"
    "ars       https://feeds.arstechnica.com/arstechnica/index"
    "nasa      https://www.nasa.gov/feed/"
    "cnn       http://rss.cnn.com/rss/edition.rss"
    "bbc       https://feeds.bbci.co.uk/news/rss.xml"
    "verge     https://www.theverge.com/rss/index.xml"
    "slashdot  http://rss.slashdot.org/Slashdot/slashdotMain"
    "github    https://github.blog/feed/"
    "xkcd      https://xkcd.com/atom.xml"
    "nyt       https://rss.nytimes.com/services/xml/rss/nyt/HomePage.xml"
)

for entry in "${FEEDS[@]}"; do
    read -r name url <<< "$entry"
    # The card asks for identity encoding, so do the same.
    if curl -s -L --fail --max-time 30 -H 'Accept-Encoding: identity' \
            -o "$OUT/$name.xml" "$url"; then
        root=$(head -c 2000 "$OUT/$name.xml" | grep -o -E '<(rss|feed|rdf:RDF)[ >]' | head -1 || true)
        printf '%-9s %8s bytes  %s\n' "$name" "$(wc -c < "$OUT/$name.xml" | tr -d ' ')" "$root"
    else
        echo "$name: download failed, skipped" >&2
        rm -f "$OUT/$name.xml"
    fi
done
