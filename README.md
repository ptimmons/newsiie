# newsiie

An RSS and Atom news reader for the Enhanced Apple //e, with the network
done by a [CoGS](https://a2cogs.com) card. Written in cc65 C with ca65
assembly for the screen.

Work in progress. Milestones 0 and 1 of [PLAN.md](PLAN.md) are done: the
splash, empty Feeds view, status bar and help box, plus finding the CoGS
card and showing its address. Nothing is fetched yet.

## Checking a CoGS card

The disk also carries `PROBE.SYSTEM`. Quit newsiie with `Open-Apple-Q`,
choose `PROBE.SYSTEM` in the ProDOS selector, and it checks the card end to
end on one screen: slot, firmware, network, clock, and data integrity in
both directions.

## Building

```sh
tools/bootstrap.sh     # checks cc65; fetches AppleCommander and ProDOS 2.4.3
make disk              # build/NEWSIIE.po, a bootable ProDOS 8 image
make run               # boot it in Virtual ][ and print the screen
make host-test         # the card protocol and formatters, tested on the Mac
```

## Layout

```
src/        the program (C, plus blit.s for the screen, cogs_io.s for the card)
tests/host/ host tests, with a software model of the card
cfg/        the cc65 linker config
tools/      bootstrap, disk builder, Virtual ][ driver (vii.sh)
```

## Keys so far

`Open-Apple-?` help, `Open-Apple-Q` quit.
