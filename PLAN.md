# newsiie — implementation plan

An RSS/Atom news reader for the Enhanced Apple //e with a CoGS card.

ZipEdit is the **design language**, not a code source: a clean 80-column
screen, an inverse status bar on the bottom row, and a MouseText-boxed help
screen. newsiie's code is its own.

**Status:** milestone 0 is done (see section 9).

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
| Feeds | RSS 2.0, RSS 1.0 (RDF), Atom 1.0, over http and https |

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
| `cogs_io.s` — probe, gated get/put, block read, resync, IRQ off | ca65 | a2chat, unchanged, plus a BULK burst read |
| `cogs.c` — frames, PING, STATUS, TIME, stream open/read/close, error text | C | new, much smaller than a2chat's (no sockets, no events to track) |
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
| 1 | `cogs_io.s` + `cogs.c` with host model tests; `cogsprobe` disk: find the card, PING, wait for link, show firmware, IP, time | host tests, then **your card** |
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
