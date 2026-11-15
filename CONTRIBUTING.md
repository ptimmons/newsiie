# Contributing to Newsiie

Thanks for your interest in Newsiie. Bug reports, test results from real
hardware, feeds that don't parse, and code are all welcome.

## Reporting a problem

Open an issue and include:

- **What you did and what happened.** A photo of the screen is often the
  quickest way to show it; the status bar at the bottom usually holds the
  message that matters.
- **Your setup:** the Apple II model and memory, the CoGS card's firmware
  version (PROBE.SYSTEM shows it), and whether you boot from the CoGS
  network disk, a hard disk or a floppy.
- **The feed's address**, if the problem is with one feed.
- **The Newsiie version**, from the title screen.

For network or card problems, the results of `PROBE.SYSTEM` and
`FETCH.SYSTEM` (on the same disk; quit Newsiie with Open-Apple-Q) help a
great deal. FETCH's `W` key tests writing to the disk.

## Building

You need macOS or Linux with [cc65](https://cc65.github.io) 2.19, Python 3
and Bash. Then:

```sh
tools/bootstrap.sh     # checks cc65; fetches AppleCommander and ProDOS 2.4.3
make disk              # build/NEWSIIE.po
make host-test         # the tests that run on your own computer
```

`make run` boots the disk in [Virtual ][](https://www.virtualii.com) on a
Mac. The emulator has no CoGS card, so `make testdisk feedsdisk` builds a
disk whose feed list also reads feeds from a second disk.

## Before you send a change

1. **`make` builds with no warnings.** The build also runs two guards, and
   both must pass:
   - `tools/cc65check.py` rejects compiler output that hits a known cc65
     miscompile (`ldaxysp` with Y=0).
   - `tools/memcheck.py` fails the link if the heap is too small for ProDOS
     to open a file. Main memory is nearly full; see PLAN.md section 5
     before adding anything sizeable.
2. **`make host-test` passes.** Add a test for what you changed where one
   fits: `tests/host/` for the card protocol, text cleaner, item store,
   word wrap and feed list, and `tests/fixtures/` for feeds (an `.xml` file
   with its expected `.out`).
3. **Try it in an emulator, and say whether you tried real hardware.**
   Behavior on a real //e with a real card is the final word; say what you
   could and couldn't test.

## Rules of the code

Newsiie runs in 64K of main memory beside ProDOS, so a few rules are firm.
PLAN.md explains each one.

- **No ProDOS call while a CoGS stream is open.** The card's network disk
  and Newsiie share the card's FIFOs. Read and write files before or after
  a fetch, never during.
- **Code in the language card** (`#pragma code-name ("LC")`) must not call
  ProDOS or the ROM. Main-memory code that calls ProDOS for it restores the
  card with `lc_ram()` before returning.
- **Files are read and written whole, once,** and every save goes through
  `io_retry()`, because writes to the network disk can fail and succeed on
  a second try.
- **Locals are static** (`-Cl`), so functions are not reentrant. Keep the
  C stack shallow; it is 512 bytes.
- **The screen is 80 columns,** with column numbers in named constants.

## Style

- C99 as cc65 accepts it, four-space indents, braces on every block, and
  fixed-width types from `port.h`.
- A comment says why, not what; each file opens with a short description.
- Text the user sees follows the manual's voice: plain words, and
  messages on the status bar in capitals.
- Commit messages: a short summary line, a blank line, then what changed
  and why.

## The manual

`docs/MANUAL.md` is written in the style of a 1980s Apple user's guide.
Keep that voice when you change it. Screenshots come from `make shots`,
which drives Virtual ][ through a demo disk; don't edit the images by hand.

## License

By contributing, you agree that your work is released under the
[MIT License](LICENSE) that covers Newsiie.
