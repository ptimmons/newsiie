# Newsiie — implementation plan

An RSS/Atom news reader for the Enhanced Apple //e with a CoGS card.

ZipEdit is the **design language**, not a code source: a clean 80-column
screen, an inverse status bar on the bottom row, and a MouseText-boxed help
screen. Newsiie's code is its own.

**Status:** version 1.0. All eight milestones are built and have run on a
real //e with a real card (section 9). Two things are not yet confirmed on
hardware: the save retries added after 0.9's network-disk failures, and
the waits for the network and clock right after power-on (the test //e
boots from the CoGS network disk, so the card is up first). `make release`
builds `build/newsiie-1.0.zip`: the disk image, docs/MANUAL.md and its
pictures.

The [FujiNet implementation plan](#12-fujinet-port) is ready for review.

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

This is how Newsiie is meant to be run, so it is a design rule, not a
caveat. The card's disk driver and Newsiie share one pair of FIFOs, and the
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
   Verified on the real card: after Newsiie and the probe each did so, ProDOS
   went on loading programs from the network disk.
4. **The volume may be read-only.** Writes to a network disk are HTTP PUTs
   to the image's origin. The CoGS Image Manager share accepts them;
   GitHub, the Internet Archive and the a2cogs catalog do not, and the
   card then reports the disk write-protected (`$2B`). Newsiie must treat
   that as a normal state: keep the feed list in memory, say
   `DISK IS WRITE-PROTECTED: FEEDS NOT SAVED` once, and carry on.
5. **Disk I/O is network I/O.** Every block costs a round trip unless it is
   in the card's cache, so files stay small and are read and written whole,
   once, rather than block by block during use.
6. **A block write can fail with $27.** Each block written is an HTTP PUT,
   and the slot ROM's wait for the `$D7` reply is bounded; reads are retried
   up to eight times, writes are not (Technical Reference rev 36, 6.3.2).
   Measured on the real card (FETCH's W test, 0.9): files of one block
   always saved, larger ones failed at random steps, with $27. Every save
   goes through `io_retry()` (feedlist.c): a $27 rewrites the whole file,
   after a second's pause, up to three times in all.

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
Newsiie's own, written with the Virtual ][ pitfalls ZipEdit documented in
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
| main `$0800–$1FFF` | `LOWBSS` (M3): the parser's big buffers, 6K, never loaded and not zeroed |
| main `$2000–$B6FF` | program: code, rodata, data, bss, then the heap (ProDOS file buffers come from it) |
| main `$B700–$BEFF` | C stack, 2K |
| aux `$0800–$BFFF` | item store, 47K, through `auxmem.s` |

**Measured in M3:** FETCH (probe UI, CoGS, parser) is 27K of code and
4.3K of read-only data, about 32K loaded, with `-Or`. The parser alone is
about 12K of code (`xml.c` 4.4K, `clean.c` 5.7K, `feed.c` 1.9K) plus 6K
of buffers in `LOWBSS`. The app adds the store, three views and the feeds
file on top of the parser and card code, so it will be tight. The levers,
in order:

1. **Keep the probes out of the app.** FETCH's own UI is 7K; the app's
   views replace it, they do not add to it.
2. **Overlays from disk, between fetches only** (section 1, rule 1):
   help, the feed editor and article save are cold and can load on demand.
3. **The language card's free 3K** (`$D400-$DFFF`, bank 2, behind the quit
   code), for code cc65 copies there at startup.
4. **Move hot C to assembly**, which is smaller as well as faster:
   `clean()`'s per-byte loop is the obvious candidate.

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
| 2 ✅ | `FETCH.SYSTEM`: stream a URL to the end and report bytes, KB/s at four read sizes, content type (XML / gzip / HTML), and result codes for http, https, a redirect, a 404; then read the boot volume through ProDOS | host tests ✅, Virtual ][ menu and disk check ✅, **real card ✅** — settled the risks below |
| 3 ✅ | Parser in C + fixtures + `make host-test`; `scan.s`; file back end; parse rate measured on the 6502 build | host tests ✅, corpus ✅, Virtual ][ ✅ (13 KB/s at 1 MHz), **real card ✅** (9.1 KB/s fetch+parse) |
| 4 ✅ | Item store in aux; Headlines screen with scrolling | host tests ✅, Virtual ][ ✅, real card ✅ |
| 5 ✅ | Article screen: cleaning, UTF-8 folding, word wrap, paging | host tests ✅, Virtual ][ ✅, real card ✅ |
| 6 ✅ | Feeds screen, `NEWSIIE.FEEDS` load/save, add/delete prompts | host tests ✅, Virtual ][ ✅, **real card ✅** (seven feeds load; an added feed survives a reboot on the network disk) |
| 7 ✅ | Live fetch wired in: progress, abort, error messages, refresh | Virtual ][ ✅ (disk feeds, no-card error); **real card ✅** (all seven feeds, Slashdot shortened, Esc mid-NASA keeps what arrived, refresh); the power-on network and clock waits not exercised |
| 8 ✅ | Read markers, save article, README and manual, release image | host tests ✅, Virtual ][ ✅, **real card**: read markers ✅ across a reboot; saving an article failed with $27 on the network disk, now retried (see "Saving on the network disk"), retry not yet re-run on the card |

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
- **The probe** is `PROBE.SYSTEM` on the same disk: quit Newsiie with
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

### Milestone 2 notes

- **Stream commands** in `cogs.c`: `cogs_stream_open`, `_read`, `_close`.
  `STREAM_OPEN` is deferred (DNS, TLS and the response header come first),
  so `cogs_request` now calls a caller-supplied `cogs_idle` hook while no
  reply has started; the hook keeps the clock and can give up, and giving
  up flushes the FIFOs so a late reply cannot be read as the next one.
- **Host tests** add streams: a deferred open, a 65,000-byte body read to
  the end in 512- and 4096-byte reads with the ring found empty on every
  other read, closing twice, reading with no stream, a 404, and an open the
  card never answers.
- **`FETCH.SYSTEM`** is on the disk beside the probe. Tests, chosen after
  checking each from the Mac with `Accept-Encoding: identity`:
  1. http, no redirect: `http://rss.cnn.com/rss/edition.rss` (~130K)
  2. https: `https://hnrss.org/frontpage` (~18K)
  3. redirect: `http://hnrss.org/frontpage` (301 to https)
  4. 404: `https://httpbin.org/status/404` (passes if refused)
  5. 75K: `https://feeds.arstechnica.com/arstechnica/index`
  6. 250K: `https://www.nasa.gov/feed/`

  `U` takes any URL, `C` cycles the read size (512 to 4096), `A` runs all
  six, and every fetch ends with a ProDOS `READ_BLOCK` of the boot volume's
  directory, done only after the stream is closed (section 1, rule 1).
- **Timing** comes from the card's uptime in `STATUS`, in whole seconds.
  The vertical-blank counter cannot time transfers: a 512-byte read takes
  about 12 ms with no chance to poll, long enough to miss a blank.
- The fetch loop keeps nothing but the first 480 bytes, so its rate is the
  transfer rate alone, before any parsing.
- **First run on the real card (2026-11-14, firmware 1.42, booted from the
  network disk), reads of 512:** http (CNN) OK 127K; https (Hacker News)
  OK 17K; **redirect http to https OK 17K**, so streams follow redirects;
  Ars Technica OK 74K. The 404 and NASA both failed the same way: the open
  succeeded, then the first `STREAM_READ` lost its reply header. The disk
  check read `/NEWSIIE` (unit `$50`, slot 5) after every fetch, so streams
  and the network disk coexist, even after a parser reset.
- **Fix 1, patient reads.** Only the open had the long wait; reads relied
  on the primitives' ~1 s byte wait. The card evidently holds the first
  read's reply until the server answers, and httpbin and nasa.gov answer
  slowly. Every stream command now waits through `cogs_idle` (60 s cap,
  seconds shown on the status bar). Host tests reproduce both behaviours.
  `cogs_lost_why` / `cogs_lost_op` now say how a frame was lost, and FETCH
  prints that, the failing read's number, and the longest single wait.
- **Fix 2, card detection.** In Virtual ][, FETCH reported a CoGS in slot 2.
  An undecoded I/O address returns the floating bus (screen memory as the
  video scanner reads it), which can read `$A2`. `cogs_probe` now needs ID
  `$A2` 32 times running with VERSION steady at 1..15; three boots since
  found no card, as they should. Real hardware can float the same way, so
  this matters beyond the emulator.
- A 404 that the card lets through (open succeeds) now shows as
  `NOT REFUSED` rather than OK.
- **Second run on the real card, after both fixes: all six pass.** CNN
  127K, Hacker News 17K over https and 17K through the redirect, the 404
  refused at open as the reference says, Ars Technica 74K, and NASA
  252,742 bytes, exactly the size curl gets on the Mac. The disk check read
  `/NEWSIIE` after each. NASA, at reads of 512: 13 s, **18.9 KB/s**, 494
  reads with data and **none empty, ring peak 255/255**, longest single
  wait 1 s. The card's ring stayed full throughout, so the Apple's read
  loop, not the network, sets the pace. Content: plain XML (RSS 2.0).
- Not explained: on the first run the 404's open succeeded and its first
  read failed; on the second it was refused at open. The fixes changed
  only the waiting, so the card or httpbin may have answered differently.
  Newsiie handles both.

### Risks settled by milestone 2

- **Redirects:** streams follow them (http to https, 301).
- **Compression:** none seen; every body arrived as plain XML.
- **TLS:** works, with the card's NTP clock set.
- **404 and other non-2xx:** refused at open (`CONN_REFUSED`, 4).
- **Speed:** 18.9 KB/s raw at 512-byte reads, limited by the Apple. The
  parser will set the real rate; 512 is enough for now, and larger reads or
  a BULK burst read are the first levers if transfer ever dominates.
- **Network disk:** streams and ProDOS block reads alternate cleanly.

### Milestone 3 notes

- **Three layers.** `xml.c` is a streaming scanner: every state survives a
  chunk boundary, text it was not asked for is skipped at `scan()` speed,
  CDATA, comments, PIs, DOCTYPE internal subsets and self-closing tags are
  handled, and nothing is fatal. `feed.c` maps RSS 2.0, RSS 1.0 (RDF) and
  Atom onto one item (title, link, date, guid, body), preferring the short
  `description`/`summary` over `content:encoded`/`content`, and hands each
  item to a sink as it closes, so memory does not grow with the feed.
  `clean.c` strips HTML, decodes entities, folds Unicode to ASCII, collapses
  whitespace and normalises dates to `YYYY-MM-DD HH:MM`. `scan.s` is the
  assembly inner loop (18 cycles a byte); `scan_host.c` is its twin.
- **Latin-1 feeds** (Slashdot declares ISO-8859-1) are turned into UTF-8 as
  they are captured, so the cleaner sees one encoding.
- **Code points are 16-bit.** 32-bit arithmetic is costly 6502 code, and
  nothing past U+FFFF folds to anything but `?`.
- **Caps per item:** title 160, link 511 (BBC's tracking links pass 400),
  guid 255, body 2000 after cleaning (3K captured).
- **Tests.** `make host-test`: 719 card-protocol checks, 66 cleaner checks,
  and 7 hand-written fixtures (RSS 2.0, Atom with xhtml content and
  prefixed elements, Latin-1 RDF, a truncated feed, an HTML page, edge
  cases, caps), each matching its expected output exactly at chunk sizes
  1, 7 and 512. `make corpus` fetches 10 live feeds (`tools/getcorpus.sh`,
  not committed: they are other people's writing) and checks that the item
  links match Python's own XML parser exactly, the output is ASCII, and
  the result does not depend on chunk size. All 10 pass.
- **On the 6502** (Virtual ][, FETCH's `F` key, feeds on a second disk):
  Hacker News 20 items, Ars Technica 20, BBC 37, xkcd 4, titles identical
  to the Mac's. **Parse rate at 1 MHz: about 13 KB/s.** Ars Technica's 75K
  took 51 s to read and parse and 45 s to read alone (the emulated floppy
  is slow), so parsing cost about 6 s; the frame counter's floor said
  4.5 s. With the network at 19 KB/s, fetch-and-parse should come to about
  7-8 KB/s: 10 s for a 75K feed, 30 s for NASA's 250K.
- **Frame timing is a floor.** Frames are counted only between tags, and
  cleaning a long body can pass several unseen; and at Virtual ]['s
  "maximum" speed frames are not a clock at all. Timing runs at "regular"
  speed against a wall clock, and the card's clock on real hardware.
- **Memory:** see section 5. FETCH's largest read is now 1024 (it was
  4096); 512 already kept the card's ring full.
- `vii.sh insert S6D2 <image>` mounts a second disk; Virtual ][ has no
  `return` key name, so `vii.sh ret` types `$0D`.
- **Real card, parsing while fetching (2026-11-14):** all six tests pass
  with the parser on. NASA, 252,742 bytes: **fetch and parse 27 s, 9.1
  KB/s**, 10 items, titles right; open 0 s, no empty reads, ring full,
  longest wait 1 s; the boot volume read fine after. Transfer alone was
  13 s in milestone 2, so parsing cost about 14 s, about 18 KB/s: faster
  than Ars Technica's 13 KB/s because NASA's long `content:encoded` bodies
  are skipped at `scan()` speed. The frame floor read 3.3 s, a large
  undercount, as expected.

### Milestone 4 notes

- **The store** (`store.c`, `auxmem.s`): one feed's items, packed in aux
  `$0800-$BFFF` (47K) as variable-length records (length, flags, a 16-bit
  hash of the guid for read markers, then title, date, link and body, each
  with its length), with a 200-entry index in main memory. It is cleared
  before each fetch and filled as the parser closes items; when the next
  item will not fit, the fetch stops and what fitted is kept. Writes use
  `RAMWRT`; reads run a 40-byte loop copied to `$0100`, since `RAMRD` would
  also redirect instruction fetches. No read ever reaches `$C000` (I/O).
- **`/RAM` is disconnected** at startup, after checking for 128K
  (`MACHID`): slot 3 drive 2 is pointed at the no-device routine and `$BF`
  taken out of `DEVLST`, but only if the vector is ProDOS's own `/RAM`
  driver (`$FF00`); anything else there is refused rather than clobbered.
  Checked in Virtual ][: `DEVLST` is `$E0 $60` afterwards.
- **`source.c`** fetches a URL (CoGS stream) or a ProDOS pathname (a file)
  through the parser into the store, with progress on the status bar and
  Esc to stop. The two never overlap (section 1, rule 1).
- **Views:** a Feeds list (built in until milestone 6) and the Headlines
  view: date and title a row, inverse bar, arrows, Open-Apple-arrows to
  page, Open-Apple-`<` `>` to the ends, Open-Apple-R to refetch, Esc back.
  The heading says when a fetch was cut short or memory filled.
- **The test build** (`make testdisk`) adds feed files on a second disk
  (`make feedsdisk`: four corpus feeds as `/FEEDS`, not committed), so the
  emulator can drive every screen without a card.
- **Host tests:** 131 store checks against an array model of aux that
  fails any access outside `$0800-$BFFF`: round trips, truncated copies,
  filling to the brim, the item cap.
- **A cc65 miscompile, found and guarded.** Headlines showed dates but no
  titles. The record in aux was right; in main memory the title had been
  written into the program's code at `$57F6`. This cc65 (reporting V2.18)
  reloaded the `store_head()` parameter `h` with `ldy #$00 / jsr ldaxysp`;
  `ldaxysp` takes the high byte at offset Y and the low at Y-1, so Y = 0
  builds a pointer from the wrong bytes. Worked around with a static copy
  of the pointer, and **every C file now compiles to assembly first and
  `tools/cc65check.py` fails the build on any `ldaxysp`/`staxysp` called
  with Y = 0**. A scan of all 22 files found only that one. The host tests
  could never have caught it: the miscompile exists only in 6502 output.
- **Memory:** the app is 30K loaded; BSS ends at `$A1BF`, leaving about 5K
  of heap below the stack. Fits, with room for milestone 5, not much more.


### Milestone 5 notes

- **Article view** (`article.c`, `wrap.c`): title, date, link (cut at 78
  with `>`), a rule, then the summary word-wrapped to 78 columns, a blank
  line between paragraphs, none between list items or after an HTML
  `<br>` (the cleaner now emits `\r` for `<br>`, `\n` for paragraphs).
  Up/Down scroll, Space and Open-Apple-arrows page, Open-Apple-`<` `>` to
  the ends, Left/Right to the previous and next item, Esc back to
  Headlines on the item last read. Text and link borrow the parser's idle
  3K buffer (`feed_scratch`).
- **Tests:** 113 wrap checks on the Mac; in Virtual ][ a 2,000-character
  body pages to `LINE 33 OF 33`, and the formatting fixture shows
  paragraphs, a list, a `<br>` and folded Unicode correctly.
- **Memory:** the app is 32K; BSS ends at `$AE8A`, about 2K below the
  stack. Milestone 6 (the feeds file) will need the levers in section 5.
- **Real card:** milestones 4 and 5 run on the user's //e (2026-11-14).

### Milestone 6 notes

- **The feed list** (`feedlist.c`): up to 21 feeds (a screen of rows), in
  aux `$B000-$BFFF` above the item store, which shrinks to `$0800-$AFFF`
  (42K). `NEWSIIE.FEEDS` is a ProDOS TXT file, one `Name|URL` a line
  (a bare URL is named after its host); comments and blank lines are read
  and dropped, CR or LF endings both work. It is read once at startup,
  before anything can open a stream, and written whole after each add or
  delete, through the parser's idle 3K buffer. A write-protected disk
  (`$2B`) is said once and the list kept in memory; other errors are shown
  with their ProDOS code. A missing file is an empty list.
- **Prompts** (`prompt.c`): a line editor on the status bar, a normal-video
  box inside the inverse bar with a block cursor, scrolling to keep the end
  in view; Return accepts, Esc cancels, Delete or Left erases. `confirm()`
  asks Y or N. Open-Apple-A asks for a URL (`https://` is added if there
  is no scheme), then a name (the host, editable); Open-Apple-D asks
  before deleting.
- **The disks:** the release disk ships `cfg/NEWSIIE.FEEDS` (seven feeds);
  the test disk is now the same program with `tests/NEWSIIE.FEEDS`, which
  adds the `/FEEDS` disk entries. The `NEWSIIE_TEST` build is gone.
- **Memory, the first lever pulled:** the app overflowed by 1.3K and needs
  about 1.3K of heap for ProDOS's file buffer. `article.c`, `wrap.c`,
  `prompt.c` and `help.c` (2.6K) now run from the language card,
  `$D400-$DFFF` bank 2, which cc65 fills at startup; the copy they load
  from is overwritten by BSS. They call neither ProDOS nor the ROM, which
  code there must not do. The store's index went to 160 items and the
  wrap table to 160 lines. BSS now ends at `$B10E`: about 1.5K of heap.
  The language card has about 400 bytes left.
- **Tests:** 38 feed-list checks on the Mac, including a real save and load
  and a Mac-made file with LF endings. In Virtual ][: the list loads from
  the file; an article and help (both from the language card) work; a URL
  typed without a scheme is added with the host as its name; delete asks
  and removes; the saved file reads back as TXT with CR endings; no file
  gives the empty list. Not checked: a write-protected disk.

### Milestone 7 notes

Fetching, progress (`FETCHING 37K, 12 ITEMS`), Esc during the transfer,
error messages and Open-Apple-R were already in place from milestone 4.
This milestone added what a live network needs:

- **Ready checks** before a fetch: up to 20 s for the card's network link
  (`WAITING FOR THE NETWORK`), and for an https URL up to 20 s for the
  card's NTP clock, without which it refuses verified TLS (`WAITING FOR
  THE CARD'S CLOCK`). Esc gives up; running out of time reports `NO
  NETWORK` or `THE CARD'S CLOCK IS NOT SET, AND HTTPS NEEDS IT`. Both are
  usual for a few seconds after power-on.
- **Esc while connecting.** A wait for the server can no longer simply be
  abandoned: the card would send its STREAM_OPEN reply later, and that
  could be taken for the next open's. So Esc is noted (`STOPPING WHEN THE
  SERVER ANSWERS`), the wait runs to the reply, and the fetch stops there.
- **A summary** when the headlines open: `20 ITEMS, 74K IN 9 S` by the
  card's clock, until the first key.
- **The status bar's network field** is refreshed after every network
  fetch, so a dropped link shows.
- **Memory:** the C stack went from 2K to 1K (with static locals it holds
  only arguments and temporaries), and the Article view's 140-line wrap
  table moved into the parser's idle buffer, which it now fills exactly
  (text 2001 + link 512 + table 560 = 3073). Main memory free went from
  1.5K to about 2.3K after this milestone's own code.
- **Checked in Virtual ][:** a disk feed opens with `20 ITEMS, 17K`; a
  network feed with no card says `NO COGS CARD`. The network waits, Esc
  while connecting and the timed summary need the real card.

### Milestone 8 notes

- **Read markers** (`readset.c`): the 16-bit guid hashes of the last 256
  items opened, 512 bytes in main memory, saved to `NEWSIIE.READ` (BIN,
  little-endian words) when leaving a feed's headlines and at quit, never
  during a fetch. `store_add()` asks the set through a hook, so items
  arrive already marked; opening an article marks its record in aux and
  adds its hash. Headlines shows a MouseText diamond (`*` without
  MouseText) for unread items. A write-protected disk stops further saves
  after the first refusal. Two guids share a hash about once in 65,536.
- **Saving an article** (`save.c`, Open-Apple-S): a name from the title's
  first 12 letters and digits plus `.MD`, editable, any ProDOS path; asks
  before replacing; writes Markdown (`# title`, date, the full link, then
  paragraphs) as TXT with CR endings. It runs from main memory because it
  calls ProDOS, and it re-selects language-card RAM (`lc_ram()`) before
  returning, since its caller, the Article view, runs from there.
- **Memory, three more levers:** probe-only code split out of the app
  (`cogs_extra.c`, `fmt_extra.c`: about 1.5K); the parser's Atom `href`
  buffer now lives in the cleaned-body buffer, since the two are never
  live together (512 bytes); `help.c` moved back to main memory to give
  the language card room for the save code. Final map: code and data to
  `$A4CA`, BSS to `$B43F`, about 1.7K of heap below the 1K stack at
  `$BB00`; the language card holds 2.6K of its 3K.
- **The manual** is docs/MANUAL.md; `make release` zips it with the disk.
- **Checked in Virtual ][:** unread diamonds; opening an item clears its
  diamond; the read set survives quitting and rebooting (`NEWSIIE.READ`,
  2 bytes for one item); Open-Apple-S offered `ITSQUOTEDAND.MD` and wrote
  the Markdown shown in the manual; the release disk boots as 0.9 with the
  seven shipped feeds. Host: 136 store checks (5 new for the read hook),
  all fixtures, all 10 corpus feeds after the `href` change.
- **Before 1.0, on the real card:** milestones 6 to 8, particularly adding
  and deleting feeds, the network waits and Esc while connecting, saving
  an article, and the read set, on the network disk (and whether its
  image accepts writes).

- **The manual, rewritten** (after milestone 8) as a period user's guide:
  chapters, numbered steps, "Important" and "By the way" notes, keys in
  capitals, a quick-reference appendix and a message list. Its 11
  figures are real screenshots: `make shots` boots `make shotsdisk` in
  Virtual ][ and captures them with `tools/shots.sh`. That disk carries
  two made-up feeds (docs/demo: the Orchard Valley Gazette, 1985) read
  from the disk, so no card, network or anyone else's writing is needed;
  its build differs only in showing a placeholder card address
  (`NEWSIIE_SHOTS` in net.c), which the manual points out.
- Feed names longer than the Feeds screen's 22-character column are now
  clipped instead of running into the URL.

### Limits, made visible and documented (after 0.9)

- **Every limit is now in the manual** (Appendix D, with a table of each
  limit and what happens at it, and a list of what Newsiie does not do),
  with notes where the reader meets them and every message in Appendix B.
- **No more silent truncation:** a summary cut at 2,000 characters (or at
  the 3K captured before cleaning) ends on a word with "[Shortened: too
  long for Newsiie to show in full.]"; a headline cut at 160 ends in
  "..."; an article past the 140-line wrap table says "[Too long to show
  in full.]"; lines of NEWSIIE.FEEDS that cannot be used (no address, one
  past 127 characters, a feed past the 21st) are counted and reported at
  startup. A feed that stops sending for 30 s now says "THE SERVER STOPPED
  SENDING" rather than blaming the card. The Headlines note "memory full"
  became "no room for more", since the 160-item cap also sets it. In the
  corpus, only Slashdot's summaries (9 of 15) are shortened, and Python
  confirms they are over 2,000 characters of text.
- **A memory bug caught on the way:** those additions left 1.1K of heap,
  and cc65's open() needs a 1K page-aligned ProDOS buffer (up to ~1.3K).
  The feed list silently failed to load ("COULD NOT READ NEWSIIE.FEEDS,
  ERROR $00"). The C stack went from 1K to 768 bytes (the deepest call
  chain uses about 150), and **tools/memcheck.py now fails the build** if
  the heap below the stack is under 1,295 bytes. It stands at 1,351: main
  memory is effectively full, and the next feature needs a lever from
  section 5 first.

### Saving on the network disk (after 0.9)

- **Real-card test:** the feed list and read markers saved, but saving an
  article said "COULD NOT SAVE, PRODOS ERROR $FF". FETCH.SYSTEM gained a
  **W** write test (six files, 200 to 6,000 bytes, written in pieces as
  Newsiie does, read back, removed). On the real card, 200 and 512 bytes
  passed every time; 513 and up failed at random steps (open, write, close,
  reopen) with $27, I/O error, before and after a fetch. In the emulator
  all pass. The cause is the card's (section 1, rule 6), not Newsiie's.
- **$FF was Newsiie's:** a failed close() was reported without its
  _oserror. Every save now reports the real code, and the article's $27
  has its own message.
- **Retries:** `io_retry()` wraps the article, feed list and read-set
  saves. Host tests cover it (45 feed-list checks).
- **Memory:** the retry code cost ~170 bytes of main memory. The C stack
  was filled with $A5 and measured in the emulator: 84 bytes deep at most
  through fetching, reading, saving with replace, and saving the read set.
  It went from 768 to 512 bytes; the heap stands at 1,434 (1,295 needed).
- **Lowercase names:** a name typed at SAVE AS with lowercase letters gave
  $40 (ProDOS 2.4 wants uppercase). The Article view now uppercases it.

## 12. FujiNet port

The first target is the Enhanced Apple //e with 128K, 80-column text,
ProDOS 8, and FujiNet connected through a compatible SmartPort interface.
The goal is a bootable FujiNet build of Newsiie that reads RSS and Atom
over HTTP and HTTPS and accepts the existing `NEWSIIE.FEEDS` file,
including its plain URLs and local ProDOS paths. Configure Wi-Fi using
FujiNet CONFIG before running Newsiie. Start testing with a local writable
ProDOS volume, then test a writable SmartPort image mounted from the
FujiNet SD card. [FujiNet Apple II setup guide](https://github.com/FujiNetWIFI/fujinet-firmware/wiki/Apple-II-%26-III-FujiNet-Quickstart-Guide)

### 1. Prove the transport with a fetch probe

- Build a FujiNet version of `FETCH.SYSTEM` using a pinned Apple II cc65
  release of `fujinet-lib`. Reuse the current probe's test cases and
  streaming parser. Start with a firmware release that includes the
  streamed HTTP GET fix shipped in v1.6.0. Record the exact firmware,
  library, SmartPort interface and slot used in each hardware run.
  [Library and release instructions](https://github.com/FujiNetWIFI/fujinet-lib) ·
  [HTTP examples](https://github.com/FujiNetWIFI/fujinet-lib-examples/tree/main/network/httpbin) ·
  [Firmware release notes](https://github.com/FujiNetWIFI/fujinet-firmware/releases/tag/v1.6.0)
- Discover the network unit by probing SmartPort devices. Build the
  FujiNet device spec from the feed URL at open time, with room for
  `N:`, all 127 URL characters, and the terminator. Request a read-only
  stream with byte translation disabled. Poll for available bytes and
  read at most 512 bytes at a time. Confirm how the chosen firmware reports
  temporary empty reads, EOF with bytes still buffered, and errors.
  Record raw device results in the probe.
  [Apple II network device implementation](https://github.com/FujiNetWIFI/fujinet-firmware/blob/master/lib/device/iwm/network.cpp)
- On hardware, check the byte count and first parsed titles against known
  RSS and Atom fixtures served over HTTP and HTTPS. Include a redirect, a
  404, a response larger than 64 KiB, and a delayed response. Check for raw
  XML, record what happens with a compressed response, and verify the
  firmware's certificate handling before documenting HTTPS behavior.
  Measure how long SmartPort calls can block. Cancel during a wait and
  during a transfer, fetch again, and read the boot volume afterward.

### 2. Wire the transport into Newsiie

- Use `src/source.c` as the boundary. Keep parser setup and finalization,
  the local-file path, progress reporting, and the item store shared.
  Put device discovery, open/read/close, and device error mapping behind
  build-selected code. Keep the CoGS wait and clock handling with the
  CoGS implementation. Update the FujiNet status field to distinguish a
  missing device, an unavailable network, and a ready device.
- Drain buffered data before treating EOF as success, and report a
  transport failure separately from invalid XML. Close the stream after
  EOF, Esc, timeout, error, or parser early stop. Keep items already
  delivered on Esc and poll for Esc between SmartPort calls. Use the
  probe's measured blocking behavior to decide what cancellation wording
  the app can support. Close before returning to any ProDOS file work.
- Add `make fujinet` and `make fujinetdisk` targets with variant-specific
  objects, maps, and output under `build/fujinet/`. Package
  `build/fujinet/NEWSIIE.po` with `NEWSIIE.SYSTEM`, the FujiNet
  `FETCH.SYSTEM`, and the normal feed list. Keep CoGS as the default
  build. Verify with both targets built in either order, without cleaning.
- Run `tools/memcheck.py` on both linked apps. The current CoGS map shows
  1,496 bytes of heap against a 1,295-byte minimum. Measure the FujiNet
  build after removing the CoGS objects, and check the SmartPort buffer,
  zero-page usage, and stack depth during fetch and file I/O. If the
  library exceeds the available space, inspect which routines the link
  pulls in and evaluate a smaller SmartPort driver before changing the
  app's memory limits.

### 3. Verify and package

- Run `make host-test` and boot both variants in an emulator for the
  local-file and missing-device paths. On real FujiNet hardware, run the
  probe cases through the app as well. Check that a dropped connection
  produces an error and that a subsequent refresh works. Exercise parser
  early stop with a feed that fills the item store.
- On both the local writable volume and the writable FujiNet-mounted
  image, edit the feed list, mark an item read, and save an article larger
  than 512 bytes after a network fetch. Reboot and verify the saved data.
  Check the error path on a read-only image.
- Document the tested hardware, firmware, setup, and any observed HTTP
  or cancellation limitations. The proposed library is GPLv3, so settle
  distribution before packaging. A release linked to it needs GPLv3
  compliance for the combined build, including corresponding source.
  If an MIT-only FujiNet build is required, use the probe to validate the
  protocol and implement the needed SmartPort calls independently.
  [Library license](https://github.com/FujiNetWIFI/fujinet-lib/blob/main/LICENSE) ·
  [GNU GPL FAQ](https://www.gnu.org/licenses/gpl-faq.html#GPLStaticVsDynamic)

After review, start milestone 1 in a separate worktree on
`codex/fujinet`. The probe results will determine the supported firmware
baseline and whether the library fits the app.

### Progress, 2026-11-23

Built on `main` instead of a worktree, and without fujinet-lib: newsiie
has its own small SmartPort driver (`src/sp.s`, `src/fuji.c`), written
from the firmware's protocol, so the FujiNet build stays MIT and the GPL
question does not arise. `src/source.c` keeps the shared half;
`src_cogs.c` and `src_fuji.c` are the two network halves, and
`net_fuji.c` gives the status field (`NO FUJINET`, `FUJINET NO WI-FI`,
`FUJINET 172.16.16.10`). `make fujinet` / `make fujinetdisk` build
`build/fujinet/NEWSIIE.po` with the app and a FujiNet `FETCH.SYSTEM`
(seven test URLs, raw device results, a boot-volume read, and `X`, a
READ experiment). The FujiNet app leaves 2,855 bytes of heap (1,295
needed); the CoGS build went from 1,496 to 1,442 with the split.

Virtual ][ (no FujiNet): both builds boot, local feed files load, and
the network path reports `NO FUJINET FOUND`; the probe fails each test
cleanly and reads the boot volume.

Bench (FujiNet firmware v1.6.1 on a Yellowstone card in slot 6, booted
from a read/write TNFS image): the driver finds NETWORK (unit 12) and
THE_FUJI (unit 13), reads Wi-Fi state, the firmware version and the
address, opens an HTTPS URL, and STATUS reports 512 bytes waiting. But
every SmartPort READ ($08) is refused by the card with $04 (carry set,
X=4), whether the parameter count is 4 or 5, the byte count 1, 64 or
512, and also after a SmartPort OPEN ($06), which is refused the same
way. The v1.6.1 firmware answers its own errors with $27 or $01, so the
refusal comes from the Yellowstone's firmware, which seems to pass only
STATUS and CONTROL (and the block calls) to a character device. v1.6.1
has no other way to fetch data (STATUS 'R' is empty there). So no feed
has been fetched over the FujiNet yet; newsiie now says "THIS SMARTPORT
CARD CANNOT READ FROM THE FUJINET ($04)" for it.

Next: run the probe through a SmartPort interface that passes READ on
(a Liron or UniDisk 3.5 card, or a //c), or with a newer Yellowstone
firmware if one adds character devices; then the rest of 1 and 3.

LIRON card in slot 5, same FujiNet (v1.6.1), read/write TNFS image,
same build: READ works with 4 or 5 parameters (the driver keeps 4).
Probe: https 15,980 bytes PASS (20 items); http-to-https redirect PASS;
404 gives device error 170 (newsiie says "refused (or not found)");
Ars Technica 75,244 bytes PASS (28 items); httpbin delay/5 returns its
280 bytes; the boot volume reads fine after each fetch. The app fetched
Hacker News (20 items, 16K) and BBC News (31 items, 23K), saved a story
(THEPOWEROFGR.MD), and wrote NEWSIIE.READ on quit, all on the
FujiNet-mounted disk.

Open:
- Large feeds: fixed on the bench 2026-11-24 by asking for at most 256
  bytes per READ (FUJI_READ_MAX, fuji.h). See "Large-feed hang" below.
- Right after a cold boot the first fetch said "NO NETWORK" though the
  adapter reported Wi-Fi; the second worked. Wait or retry once.
- A warm reboot (Ctrl-Open-Apple-Reset) with the LIRON stops at the
  "Apple //e" banner; `a2h fujinet --boot --cold` works.
- The probe eats any key while it watches for Esc, so queued test
  numbers are lost.

### Large-feed hang, 2026-11-24

The probe now redraws a line before every SmartPort call (which call,
reads, bytes, waiting, bytes asked), and has P (parse on/off) and C (READ
size 256/64/512). `make fujinettestdisk PROBE_HOST=ip:port` (now `make testdisk
PROBE_HOST=ip:port`, for FNFETCH) adds two
tests that fetch from a test server on the bench Mac.

- With 512-byte READs, CNN over the internet died partway every time: at
  54K inside READ 107 with the FujiNet rebooting under it (its web page's
  uptime went back to 0), so the LIRON waits forever; and with parsing off,
  at 75K with READ returning $27 while STATUS said 512 waiting,
  connected 0.
- Not the parser: the 6502 build reads the same 127K from disk. Not the
  servers: CNN, NASA and Ars all send whole to a Mac reading at 2.5K/s
  through a 4K window. Not chunking or CNN's bytes: the same response
  replayed from the Mac (its headers and 32K chunks) arrives whole at 512.
  Not time: 64-byte READs ran six minutes and got all 130,144 bytes.
- 256 and 64 both arrived whole from the internet; 512 never did.
- Why, from the v1.6.1 source: for HTTP, "waiting" is the body length
  still to come (content or chunk length minus bytes read), not data in
  hand. fnHttpClient holds one 512-byte piece; a READ for more than its
  rest waits on the network inside the SmartPort call, and if the HTTP
  task ends while read() is waiting, read() hands back stale data and
  notifies a task that is gone, which asserts and reboots the ESP32.
  Firmware PR #1586 (after v1.6.1) fixes that crash; smaller READs avoid
  most of the waiting either way.
- With 256, the app fetched CNN whole on the //e: 50 items, 127K. But
  256 is not a cure: later the same day (the one-disk build below, same
  driver code) CNN stopped at 96K with the FujiNet rebooting, so 256 makes
  the crash rarer, not impossible. The fix is the firmware's (#1586);
  until a release carries it, a FujiNet on v1.6.1 can hang on big feeds.

### One disk for both: the network driver loaded at startup, 2026-11-24

PT asked for one build that picks the network at runtime. The app's
three calls into the network (net_check, source_stream,
source_net_error) now go through a table at the start of a driver slot,
$0FD0 bytes under the 512-byte C stack (cfg/newsiie-app.cfg, which counts
the slot in __STACKSIZE__ so cc65's heap stops below it). Each driver is
linked with the app, so it calls the app freely, but into its own file
(the COGS* and FUJI* segments): NEWSIIE.COGS ($FCD with its BSS, the
larger) and NEWSIIE.FUJI. netdrv.c loads NEWSIIE.COGS and calls its find;
if no CoGS card, NEWSIIE.FUJI the same way; if neither, the status bar
says NO NETWORK CARD and fetches say NO COGS CARD OR FUJINET FOUND. A
table carries a stamp (the app's __ONCE_RUN__ as linked), so a driver
from another build is refused. tools/drvcheck.sh fails the build if the
app imports a driver's symbol or one driver imports the other's.

Memory: the CoGS build had 1,442 bytes of heap (147 over the 1,295 a
file open needs). The slot costs what the CoGS code did, plus the loader
and table (about 150 bytes, after a first C version with function
pointers cost 471). To make room, ui.c's code moved into language-card
RAM and two buffers always written before use (main.c's note,
source_msg) into LOWBSS. The heap is 1,329: 34 bytes over. Anything new
in main memory now needs room found for it first.

The separate FujiNet build is gone: one `make disk` carries NEWSIIE.SYSTEM,
both drivers, PROBE, FETCH and FNFETCH (the FujiNet probe, was the FujiNet
build's FETCH.SYSTEM). The splash shows the build's commit.

Tested: in Virtual ][ (no card), both drivers load and find nothing, the
status bar says NO NETWORK CARD, a disk feed loads, a story saves and
NEWSIIE.READ is written. On the //e through the LIRON, NEWSIIE.FUJI loads
(FUJINET 172.16.16.10 on the status bar) and fetched BBC News (32 items)
and Hacker News (20); CNN hit the firmware crash above at 96K. The CoGS
path is untested on hardware: PT will put the CoGS card back in.

