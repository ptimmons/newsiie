# Newsiie

An RSS and Atom news reader for the Enhanced Apple //e, with the network
done by a [CoGS](https://a2cogs.com) card. Written in cc65 C with ca65
assembly for the screen.

Version 1.0: all eight milestones of [PLAN.md](PLAN.md) are built. Pick a
feed, read its headlines and summaries, add and delete feeds, keep track
of what you have read, and save articles for ZipEdit. See
[the manual](docs/MANUAL.md).

## Checking a CoGS card

The disk also carries two test programs. Quit Newsiie with `Open-Apple-Q`
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
make release           # build/newsiie-<version>.zip: the disk and the manual
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
edit it in Newsiie or in ZipEdit.

## Contributing and license

Bug reports, real-hardware test results and changes are welcome; see
[CONTRIBUTING.md](CONTRIBUTING.md). Newsiie is released under the
[MIT License](LICENSE).
