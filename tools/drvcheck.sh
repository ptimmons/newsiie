#!/bin/bash
# drvcheck.sh "<app objects>" "<driver objects>" -- the driver slot's rule
# (src/netdrv.h): the app may reach a driver only through the table at the
# slot's start, so no app object may import a symbol a driver exports; and
# the drivers share the slot, so neither may import the other's.
# Fails, naming each such symbol, if the rule is broken.
set -euo pipefail

names() {   # names() imports|exports objects... : the symbols, one a line
    local what=$1; shift
    [ $# -gt 0 ] || return 0
    od65 --dump-$what "$@" | sed -n 's/^ *Name: *"\(.*\)"$/\1/p' | sort -u
}

app=($1)
cogs=() fuji=()
for o in $2; do
    case "$o" in
        */drv/cogs/*) cogs+=("$o") ;;
        */drv/fuji/*) fuji+=("$o") ;;
    esac
done

bad=0
check() {   # check label "<objects importing>" "<objects exporting>"
    local hits
    hits=$(comm -12 <(names imports $2) <(names exports $3))
    if [ -n "$hits" ]; then
        echo "drvcheck: $1:" $hits >&2
        bad=1
    fi
}
check "the app uses the CoGS driver directly" "${app[*]}" "${cogs[*]}"
check "the app uses the FujiNet driver directly" "${app[*]}" "${fuji[*]}"
check "the CoGS driver uses the FujiNet driver" "${cogs[*]}" "${fuji[*]}"
check "the FujiNet driver uses the CoGS driver" "${fuji[*]}" "${cogs[*]}"
exit $bad
