# newsiie

An RSS and Atom news reader for the Enhanced Apple //e, with the network
done by a [CoGS](https://a2cogs.com) card. Written in cc65 C with ca65
assembly for the screen.

Work in progress: milestone 0 of [PLAN.md](PLAN.md) is done (splash, empty
Feeds view, status bar, help box, bootable disk). No networking yet.

## Building

```sh
tools/bootstrap.sh     # checks cc65; fetches AppleCommander and ProDOS 2.4.3
make disk              # build/NEWSIIE.po, a bootable ProDOS 8 image
make run               # boot it in Virtual ][ and print the screen
```

## Layout

```
src/        the program (C, plus blit.s for the 80-column screen)
cfg/        the cc65 linker config
tools/      bootstrap, disk builder, Virtual ][ driver (vii.sh)
```

## Keys so far

`Open-Apple-?` help, `Open-Apple-Q` quit.
