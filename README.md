# newsiie

An RSS and Atom news reader for the Enhanced Apple //e, with the network
done by a [CoGS](https://a2cogs.com) card. Written in cc65 C with ca65
assembly for the screen.

Work in progress. Milestones 0 to 6 of [PLAN.md](PLAN.md) are done: the
splash, empty Feeds view, status bar and help box; finding the CoGS card;
fetching feeds through it; and a feed parser for RSS 2.0, RSS 1.0 and Atom.
Milestone 4 adds the reader itself: pick a feed, and its headlines
arrive in auxiliary memory and scroll on screen. Reading articles is next.

## Checking a CoGS card

The disk also carries two test programs. Quit newsiie with `Open-Apple-Q`
and choose one in the ProDOS selector.

- `PROBE.SYSTEM` checks the card end to end on one screen: slot, firmware,
  network, clock, and data integrity in both directions.
- `FETCH.SYSTEM` downloads test feeds through the card's byte streams,
  parses them as they arrive, and reports size, speed, the feed's name, its
  item count and first headlines; then it checks that ProDOS can still read
  the boot volume. Press `A` to run everything, `P` to turn parsing off, and
  `F` to parse a feed file from a disk instead (no card needed).

## Building

```sh
tools/bootstrap.sh     # checks cc65; fetches AppleCommander and ProDOS 2.4.3
make disk              # build/NEWSIIE.po, a bootable ProDOS 8 image
make run               # boot it in Virtual ][ and print the screen
make host-test         # card protocol, text cleaner and feed fixtures, on the Mac
make corpus            # the parser against ten live feeds
```

## Layout

```
src/        the program: C, plus assembly for the screen (blit.s), the card
            (cogs_io.s) and the parser's inner loop (scan.s)
tests/      host tests (a software model of the card) and feed fixtures
cfg/        the cc65 linker config
tools/      bootstrap, disk builder, Virtual ][ driver (vii.sh)
```

## Keys so far

Feeds: arrows choose, `Return` fetches and opens the feed, `Open-Apple-A`
adds a feed, `Open-Apple-D` deletes one. Headlines: arrows move,
`Open-Apple`-arrows page, `Open-Apple-<` and `>` go to the ends, `Return`
reads the item, `Open-Apple-R` fetches again, `Esc` goes back. Article:
arrows scroll, `Space` pages, `Left` and `Right` step through items, `Esc`
goes back. Anywhere: `Open-Apple-?` help, `Open-Apple-Q` quit.

The feed list is the text file `NEWSIIE.FEEDS`, one `Name|URL` a line;
edit it in newsiie or in ZipEdit.
