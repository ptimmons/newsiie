# newsiie — implementation plan

An RSS/Atom news reader for the Enhanced Apple //e with a CoGS card.

ZipEdit is the **design language**, not a code source: a clean 80-column
screen, an inverse status bar on the bottom row, and a MouseText-boxed help
screen. newsiie's code is its own.

**Status:** milestones 0 and 1 are done, and milestone 1 is verified on a
real card (see section 9).

Sources read for this plan: `~/repos/zipedit` (CLAUDE.md, docs/design.md,
lang/en.txt, src layout, reference screenshots), the CoGS Technical Reference
(firmware 1.39, rev 33) chapters 2–5, 8, 9, 12 and Appendix A, and the CoGS
code in `~/repos/a2chat` and `~/repos/smbget`.

## 1. Target

| | |
|---|---|
| Machine | Enhanced Apple //e, 128K, 80 columns (IIgs in 8-bit mode should work) |
| OS | ProDOS 8, launched as `NEWSIIE.SYSTEM` |
| Network | CoGS card, any slot, firmware 1.39 or later, already joined to a network |
| Boot | **from the CoGS network disk** (the primary setup), or any local ProDOS volume |
| Feeds | RSS 2.0, RSS 1.0 (RDF), Atom 1.0, over http and https |

### Booting from the CoGS network disk

This is how newsiie is meant to be run, so it is a design rule, not a
caveat. The card's disk driver and newsiie share one pair of FIFOs, and the
slot ROM, while it waits for a block, discards any frame that is not its
reply. So:

1. **No ProDOS call while a stream is open.** A fetch is: open the stream,
   pull the whole feed into the aux item store, close the stream. Files
   (the feed list, read markers, saved articles) are read or written only
   before or after that, never during.
2. **Nothing unsolicited from the card.** Streams are pull-only, which is
   one more reason to prefer them over the `HTTP` command's DATA events. If
   the `HTTP` fallback is ever needed, it runs without `STREAM` and is fully
   drained before any disk call.
3. **No soft reset.** `cogs_open` resets only the frame parser and FIFOs.
   Verified on the real card: after newsiie and the probe each did so, ProDOS
   went on loading programs from the network disk.
4. **The volume may be read-only.** Writes to a network disk are HTTP PUTs
   to the image's origin. The CoGS Image Manager share accepts them;
   GitHub, the Internet Archive and the a2cogs catalog do not, and the
   card then reports the disk write-protected (`$2B`). newsiie must treat
   that as a normal state: keep the feed list in memory, say
   `DISK IS WRITE-PROTECTED: FEEDS NOT SAVED` once, and carry on.
5. **Disk I/O is network I/O.** Every block costs a round trip unless it is
   in the card's cache, so files stay small and are read and written whole,
   once, rather than block by block during use.

Not in v1: Uthernet II, the ][+ / 40 columns, fetching the linked web page,
images, podcasts, OPML. Screen columns live in named constants, so 40
columns is not ruled out later.

## 2. The one decision that shapes everything: let the card do the network

The card offers three ways to get a feed. The plan uses the third.

| Path | Verdict |
|---|---|
| `CONNECT`/`SEND` sockets + our own HTTP (what a2chat does) | No. We would write HTTP, redirects and chunking ourselves, and a2chat found the card drops data when its 16 KB FIFO fills. |
| `HTTP` ($20) with `RETAIN` + `JSON_GET` | No. Feeds are XML, and the retained body is capped at 32 KB. |
| **`STREAM_OPEN` / `STREAM_READ` / `STREAM_CLOSE` ($70–$72)** | **Yes.** Pull model: the card does DNS, TLS and the GET, holds the body in a 32 KB ring, and uses TCP flow control to pace the server. The Apple asks for bytes when it is ready, so a slow parser cannot lose data and feed size is unlimited. |

So the Apple never sees HTTP. A fetch is: open the URL, read 512 bytes at a
time into the parser, close. `STREAM_READ` never blocks and nothing arrives
unasked, which also makes the "one owner of the FIFO" rule easy to keep.

Fallback if streams disappoint on real hardware (see risks): `HTTP` with the
`STREAM` flag, whose redirect handling is documented, reading only as fast as
we can and stopping early.

## 3. Language and toolchain

**Decided: cc65 C (`cl65 -t apple2enh`), with ca65 assembly for the hot
paths.** Same toolchain as a2chat and smbget; `cl65` is already installed.

What this buys: the CoGS register code is reused as is, the parser and the
CoGS framing compile natively on the Mac for fast tests, and the logic is
quicker to write. What it costs: plain C is too slow for the two inner
loops, which is why they are assembly.

| Module | Language | Origin |
|---|---|---|
| `cogs_io.s` — probe, gated get/put, resync, IRQ off | ca65 | **built (M1)**, adapted from a2chat; BULK burst read to come with streams |
| `cogs.c` — frames, PING, STATUS, TIME, LOCALTIME, RANDOM, HASH, error text | C | **built (M1)**; stream open/read/close come in M2 |
| `slot.c`, `net.c`, `tick.c`, `fmt.c`, `probe.c` | C | **built (M1)**: slot scan, status-bar link field, VBL frame counter, number/date formatting, the probe program |
| `auxmem.s` — aux read/write/copy, `/RAM` disconnect | ca65 | a2chat / smbget |
| `blit.s` + `screen.c` — 80-column row blit, inverse, MouseText, row table | ca65 + C | **built (M0)**; direct screen writes, no `conio` |
| `kbd.c`, `status.c`, `splash.c`, `help.c`, `feeds.c` | C | **built (M0)** |
| `scan.s` — skip-to-`<`, copy-text-until-delimiter | ca65 | new; the bytes-per-second loop |
| `xml.c`, `feed.c`, `clean.c` — scanner states, feed mapping, text cleaning | C | new, also built natively for tests |
| `store.c`, `wrap.c` — item store in aux, word wrap | C | new |
| `heads.c`, `article.c`, `prompt.c` | C | new |
| `prodos.c` — feeds file, save article | C | a2chat's MLI wrapper, trimmed |

Build and emulator tooling (`tools/bootstrap.sh`, `mkdisk.sh`, `vii.sh`) is
newsiie's own, written with the Virtual ][ pitfalls ZipEdit documented in
mind (eject before rebuilding a mounted image, set keyboard delay after
restart, verify the image carries the binary).

CoGS rules carried over from a2chat: every DATA read gated on STATUS or BULK,
`abs,X` access only, IRQs off around a frame round trip, flush with CONTROL
bits 3 and 2 and never the soft reset (which would kill a CoGS network disk
ProDOS may be using).

Size discipline, as in a2chat: `#pragma static-locals`, no `printf`/`stdio`,
no `conio`, a hand-rolled number formatter.

## 4. Screens

Rows 1–23 content, row 24 the inverse status row, prompts and errors take the
status row over, MouseText box for help, a splash that waits for a key.
Body text is mixed case; the status bar is uppercase.

1. **Feeds** — one feed per row, inverse selection bar, unread count at the
   right. Status row: `NEWSIIE   12 FEEDS   CoGS 192.168.1.60   OA-? HELP`.
2. **Headlines** — one item per row: read marker, date, title clipped to the
   width. Status row: feed name, `3 OF 40`.
3. **Article** — title, date, link, then the summary word-wrapped to 78
   columns. Status row: feed name, page position.
4. **Help** — a two-column MouseText box over the body rows, drawn from a
   table in `help.c` (built in M0 with the full planned keymap).

Keys: arrows move, `Return` opens, `Esc` goes back, `Space` / `OA-down` page,
`OA-R` refresh (in Feeds: all; in Headlines: this one), `OA-A` add feed
(status-row prompt for a URL, `ESC CANCELS`), `OA-D` delete feed (asks first),
`OA-S` save the article as a ProDOS text file ZipEdit can open, `OA-?` help,
`OA-Q` quit. Fetching shows `-- FETCHING 37K --` on the status row, updated per
chunk, and `Esc` aborts. Failures are short status-bar messages: `COGS: DNS FAILED`,
`COGS: NO NETWORK`, `HTTP ERROR`, `NOT A FEED`.

## 5. Memory

| Range | Use |
|---|---|
| main `$0800–$1FFF` | ProDOS I/O buffer, stream chunk buffer (6-byte header + 512 data) |
| main `$2000–~$7FFF` | program: code, rodata, data, bss (24K budget; a custom linker config like a2chat's) |
| main `~$8000–$BEFF` | item index, feed table, wrap line index, C stack; no `malloc` heap |
| aux `$0800–$BFFF` | item store, 47K, through `auxmem.s` |

If the program outgrows 24K, the first move is a2chat's: put cold code
(feed editing, help) in an overlay or the language card.

`/RAM` is disconnected at startup, since ProDOS puts it in the same aux
memory (patch the slot 3 drive 2 vector and remove `$BF` from `DEVLST`). Only
one feed's items are in memory at a time. Each item is a packed record: flags,
date, title (capped ~160 chars), link (capped 255), body (capped ~3K). When
the store or the item cap is reached the stream is closed early, which is
also what keeps a huge feed from taking minutes.

## 6. The feed parser

A streaming state machine fed one chunk at a time, in three layers:

1. **XML scanner** — tags, attributes (only Atom's `link href`), text,
   `<![CDATA[ ]]>`, comments, entities (`&amp; &lt; &gt; &quot; &apos; &#n;
   &#xn;` and a short named table). Tracks depth and the current element;
   everything not wanted is skipped at full speed.
2. **Feed mapping** — `channel/title`; `item` or `entry`; `title`; `link`
   (text or `href`); `description` / `summary` / `content` /
   `content:encoded`; `pubDate` / `updated` / `dc:date`; `guid` / `id`.
3. **Text cleaning** for titles and bodies — a second entity pass and HTML
   tag stripping (descriptions are usually escaped HTML), `<p> <br> <li>` to
   line breaks, whitespace collapsed, and UTF-8 folded to what the //e can
   draw: curly quotes, dashes, ellipsis and accented letters to their ASCII
   neighbours, anything else to `?`.

The three layers are plain portable C with no Apple dependencies, so the same
source builds natively on the Mac. A fixture set of real feeds (RSS 2.0, RDF,
Atom, CDATA-heavy, a UTF-8-heavy one, a malformed one) with expected output
is run through it by `make host-test`, fed in awkward chunk sizes (1, 7, 512
bytes) to prove no state is lost at a chunk boundary. `scan.s` has a C twin
for the native build.

## 7. Files on disk

- `NEWSIIE.SYSTEM` — the program.
- `NEWSIIE.FEEDS` — plain text, one `NAME|URL` per line, editable in ZipEdit.
  Read once at startup and written once after a change; never while a
  stream is open (section 1).
  Ships with a few known-good default feeds.
- `NEWSIIE.READ` — later milestone: a 16-bit hash of each read item's guid.

## 8. Testing

Three levels:

1. **Native (`make host-test`)** — the parser against fixtures, and `cogs.c`
   against a small software model of the card's registers and stream
   commands, as smbget's `tests/host/test_cogs.c` does. Runs in seconds and
   covers most of the logic.
2. **Virtual ][** — there is no CoGS in Virtual ][, so the parser pulls
   chunks through one `get_chunk` function pointer with two back ends: the
   CoGS stream and a ProDOS file. A test build reads fixture feeds off the
   disk image, and `tools/vii.sh` (screen text, screenshots, RAM dumps)
   drives a suite that covers the item store and every screen on the real
   6502 build.
3. **Your real card** — at three planned checkpoints (milestones 1, 2, 7),
   the first two as small standalone probe disks.

There is no CoGS-enabled MAME/Ample on this Mac. If you install the model
(reference ch. 12), level 2 can drive the real fetch path headless too.

## 9. Milestones

| # | Deliverable | Verified by |
|---|---|---|
| 0 ✅ | Repo scaffold, Makefile, linker config, screen primitives, splash, empty Feeds view, status bar, help box, bootable disk | Virtual ][ (splash, Feeds, help, OA-Q to ProDOS) |
| 1 ✅ | `cogs_io.s` + `cogs.c` with host model tests; `PROBE.SYSTEM`: find the card, PING, wait for link, show firmware, IP, time, data integrity both ways | host tests ✅, Virtual ][ no-card path ✅, **real card ✅** |
| 2 | `fetchprobe` disk: `STREAM_OPEN` a URL, dump bytes to the screen, report size, KB/s, and result codes for http, https, a redirecting URL, a 404 | **your card** — settles the risks below |
| 3 | Parser in C + fixtures + `make host-test`; `scan.s`; file back end; parse rate measured on the 6502 build | host tests, Virtual ][ |
| 4 | Item store in aux; Headlines screen with scrolling | suite |
| 5 | Article screen: cleaning, UTF-8 folding, word wrap, paging | suite |
| 6 | Feeds screen, `NEWSIIE.FEEDS` load/save, add/delete prompts | suite |
| 7 | Live fetch wired in: progress, abort, error messages, refresh | **your card** |
| 8 | Read markers, save article, README and manual, release image | suite + card |

Milestones 1–2 and 3 are independent and could run in parallel.

### Milestone 0 notes

- Program is 3.2K. Quitting returns cleanly to ProDOS's Bitsy Bye.
- **cc65 runs the program with language-card RAM readable** (`bit $C080` in
  its startup) and restores the ROM only on exit. Any call into the monitor
  ROM (`$F800–$FFFF`) must bracket itself with `bit $C082` / `bit $C080`,
  as `scr_done` does; without it OA-Q crashed into the monitor at `$DEDE`.
- **Box edges:** `$5A` (stroke on the right edge of its cell) on the left
  side and `$5F` (left edge) on the right side, so every horizontal rule
  spans just the interior and meets both sides. The top edge is a normal
  `_` along the foot of the row above. Checked by zooming the screenshot.
- Only `OA-?` and `OA-Q` work yet; the help box already shows the full
  planned keymap so the layout can be judged.
- Not verified: an unenhanced //e (no MouseText; falls back to `| - _`), and
  real hardware.

## 10. Risks to settle at milestone 2

- **Redirects on streams.** The reference documents "up to three redirects"
  for `HTTP` but says only that a non-2xx stream open fails. Many feed URLs
  redirect. If streams do not follow them, use the `HTTP` fallback or have
  `OA-A` resolve the final URL once and store it.
- **Compression.** `HTTP` sends `Accept-Encoding: identity`; streams
  presumably do too. A server that gzips regardless would be unreadable.
- **TLS needs the card's clock.** A verified https connect is refused until
  `TIME` is nonzero, so startup waits for NTP as well as the link.
- **One network stream at a time.** A stream left open by something else
  returns BUSY; startup closes handle 0 first.
- **Feeds that block the card's User-Agent** (`CoGS/<version> …`) cannot be
  fixed from the Apple side with streams; `HTTP` allows extra header lines.
- **Speed is an estimate.** a2chat parses JSON in C at about 3 KB/s. With the
  skip and copy loops in assembly I project 5–10 KB/s on feed XML, where most
  bytes are skipped; that is a projection, not a measurement. Milestone 2
  gives the raw transfer figure and milestone 3 the parse rate. If it is too
  slow, more of `xml.c` moves into `scan.s`, and the item cap bounds the wait
  either way.
- **Program size.** cc65 code is bulky; the 24K budget is watched from
  milestone 0, with an overlay as the planned relief.

## 11. Decisions

1. Language: **cc65 C**, decided.
2. Open: is v1 "summaries from the feed" acceptable, with full-article
   fetching left for later? (It would need a text-extraction proxy on the LAN
   or internet.) The plan assumes yes.

### Milestone 1 notes

- **Card code.** `cogs_io.s` is a2chat's register layer, trimmed. `cogs.c`
  sends one command and reads frames until its reply, skipping events and
  unrelated replies; an `ERROR` with socket `$FF` means the card refused
  the command. If a frame stops partway, it resets the card's parser and
  both FIFOs (CONTROL `$08` then `$04`, never the soft reset).
- **Host tests:** `make host-test` runs 46 checks against a model of the
  card: every command, events and a socket `ERROR` queued ahead of a
  reply, replies longer and shorter than expected, an unknown opcode, a
  reply cut off partway, no reply at all, a stray byte, and the number and
  date formatters against Python's `datetime`.
- **The probe** is `PROBE.SYSTEM` on the same disk: quit newsiie with
  `OA-Q` and pick it in the ProDOS selector. It scans the slots, resets
  the card, PINGs, waits up to 20 s for a link, reads `STATUS`, `TIME`
  and `LOCALTIME`, checks data both ways (eight 255-byte `RANDOM`
  replies; SHA-256 of `"abc"` and of 1000 known bytes against digests
  from Python's `hashlib`), and times 100 PINGs. One screen, meant to be
  photographed.
- **The app** finds the card while the splash is up and shows its address
  (or `NO COGS CARD`, `JOINING`, `NO NETWORK`) on the status bar.
- **Disk II safety:** a slot whose ROM is a Disk II's boot ROM is never
  probed, since its `$C0nX` switches move the head. Verified in Virtual ][:
  slot 6 is skipped and ProDOS keeps loading programs after both scans.
- **Virtual ][ has no CoGS card,** so only the no-card path is tested in
  the emulator.
- **Real card, 2026-11-14: all checks passed.** Enhanced //e, card in
  slot 5, firmware **1.42, protocol $05** (newer than the 1.39 / $04 the
  reference describes; nothing here depends on either value). Wi-Fi link
  up, 172.16.16.107, -53 dBm. 134K heap free, one socket already open
  before we touched it. TIME synced from NTP, so verified https is
  possible. LOCALTIME equals UTC: the card's timezone offset is 0.
  RANDOM 8 x 255 and both SHA-256 digests correct. 100 PINGs in 15
  frames, about 2.5 ms a round trip (each round trip is shorter than a
  frame, so the once-per-PING tick poll cannot miss frames). The app's
  status bar showed `COGS 172.16.16.107`.
- **For milestone 2:** this machine boots from the CoGS network disk
  (confirmed), so the rules in section 1 apply from the first fetch. The
  open socket is not ours; `STREAM_OPEN` may also find the network stream
  busy, so open should close handle 0 first. `fetchprobe` should run from
  the network disk too, and report whether ProDOS still loads after a
  stream has been opened, read and closed.
- **Size:** NEWSIIE.SYSTEM 8.7K, PROBE.SYSTEM 10.8K. The app links all of
  `cogs.c` and the 32-bit date code it does not use yet, since the linker
  takes whole object files; worth splitting if space gets tight.
- `vii.sh`: Virtual ][ has no `return` key name; `vii.sh ret` types `$0D`.

