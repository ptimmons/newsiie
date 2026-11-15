#!/bin/bash
# shots.sh <image.po> <dir> -- capture the manual's screenshots.
#
# Boots the screenshot disk (make shotsdisk) in Virtual ][ and walks
# through the program, saving a PNG at each step. The disk's feed list is
# the shipped one, so Chapter 1 and 3 show what a reader will see; for
# Chapter 2 the script adds a made-up feed kept on the disk itself, so no
# card or network is needed and no one else's writing appears in the manual.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
V="$ROOT/tools/vii.sh"
IMG="${1:?usage: shots.sh <image.po> <dir>}"
OUT="${2:?usage: shots.sh <image.po> <dir>}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

snap() {
    sleep 1
    "$V" snap "$OUT/$1.png" >/dev/null
    echo "  $1.png"
}
down() {
    local i
    for ((i = 0; i < $1; i++)); do "$V" key "down arrow" >/dev/null; done
}

"$V" boot "$IMG" >/dev/null
"$V" await "PRESS ANY KEY" 30
snap title

"$V" text " " >/dev/null
"$V" await "7 feeds" 15
snap feeds

"$V" oa "?" >/dev/null
"$V" await "KEYBOARD COMMANDS" 10
snap help
"$V" text " " >/dev/null
"$V" await "7 feeds" 10

# Adding a feed: the URL, then the name it offers.
"$V" oa a >/dev/null
"$V" await "FEED URL:" 10
"$V" text "hnrss.org/newest" >/dev/null
sleep 3
snap add-url
"$V" ret >/dev/null
"$V" await "NAME:" 10
snap add-name
"$V" key esc >/dev/null
"$V" await "7 feeds" 10

"$V" key "down arrow" >/dev/null
"$V" oa d >/dev/null
"$V" await "DELETE" 10
snap delete
"$V" text "n" >/dev/null
"$V" await "7 feeds" 10

# The made-up feed, added the way the manual says; it becomes the eighth
# and the bar moves to it.
"$V" oa a >/dev/null
"$V" await "FEED URL:" 10
"$V" text "/NEWSIIE/GAZETTE.XML" >/dev/null
"$V" ret >/dev/null
"$V" await "NAME:" 10
"$V" text "Orchard Valley Gazette" >/dev/null
"$V" ret >/dev/null
"$V" await "8 feeds" 10

# Fetching, at a real //e's speed so the progress shows.
osascript -e 'tell application "Virtual ][" to set speed of (last machine) to regular' >/dev/null
"$V" ret >/dev/null
sleep 2
snap fetching
osascript -e 'tell application "Virtual ][" to set speed of (last machine) to maximum' >/dev/null
"$V" await "8 ITEMS" 60
snap headlines-new

# An article with a list, then save it.
down 2
"$V" ret >/dev/null
"$V" await "Item 3 of 8" 15
snap article
"$V" oa s >/dev/null
"$V" await "SAVE AS:" 10
snap save
"$V" key esc >/dev/null
sleep 1

# Read two, and the headlines show which.
"$V" key "left arrow" >/dev/null
"$V" await "Item 2 of 8" 10
"$V" key esc >/dev/null
"$V" await "2 OF 8" 10
snap headlines

echo "screenshots in $OUT"
