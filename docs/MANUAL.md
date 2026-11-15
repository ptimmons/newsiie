# newsiie manual

newsiie reads RSS and Atom news feeds on an Enhanced Apple //e. The CoGS
card does the networking: it fetches each feed over http or https, and
newsiie turns it into headlines and summaries on an 80-column screen.

## What you need

| | |
|---|---|
| Computer | Enhanced Apple //e with 128K and an 80-column card (an unenhanced //e works, without MouseText) |
| Network | a CoGS card in any slot, already joined to your network on its own configuration screen |
| Disk | ProDOS 8; the newsiie disk can be a floppy, a hard disk, or the CoGS card's network disk |

newsiie uses the //e's auxiliary memory for the feed being read. ProDOS
puts its `/RAM` disk there on a 128K machine, so newsiie disconnects
`/RAM` when it starts. Anything you kept on `/RAM` is lost; it comes back,
empty, the next time you start up.

## Starting

Boot the newsiie disk. A title screen appears; press any key. While it is
up newsiie looks for the CoGS card, and the bar at the bottom of the screen
then shows the card's network address, `JOINING` while the card is still
connecting, or `NO COGS CARD`.

## The screens

**Feeds** lists your feeds. Choose one with the arrow keys and press
`Return`: newsiie fetches it, showing its progress on the bar at the
bottom (`FETCHING 37K, 12 ITEMS`), and then opens its headlines. `Esc`
stops a fetch; whatever arrived so far can still be read.

**Headlines** lists the feed's items, newest first as the feed gives
them, with the date and title. A diamond at the left marks an item you
have not read yet. The bar says what the fetch brought, such as `20 ITEMS,
74K IN 9 S`, until you press a key. Press `Return` to read an item.

**Article** shows one item: its title, date and link, then its summary.
newsiie shows the summary the feed carries; it does not fetch the web page
the link points to.

## Keys

| Where | Key | Does |
|---|---|---|
| Feeds | Up, Down | choose a feed |
| | `Return` | fetch it and show its headlines |
| | `Open-Apple-A` | add a feed |
| | `Open-Apple-D` | delete the chosen feed (it asks first) |
| Headlines | Up, Down | choose an item |
| | `Open-Apple`-Up, Down | a page at a time |
| | `Open-Apple-<`, `>` | the first, the last item |
| | `Return` | read the item |
| | `Open-Apple-R` | fetch the feed again |
| | `Esc` | back to Feeds |
| Article | Up, Down | scroll a line |
| | `Space`, `Open-Apple`-Up, Down | a page at a time |
| | `Open-Apple-<`, `>` | the top, the end |
| | Left, Right | the previous, the next item |
| | `Open-Apple-S` | save the item as a text file |
| | `Esc` | back to Headlines |
| Anywhere | `Open-Apple-?` | the list of keys |
| | `Open-Apple-Q` | quit to ProDOS |
| While fetching | `Esc` | stop |

When newsiie asks a question on the bottom bar, type your answer and
press `Return`; `Delete` or Left erases, and `Esc` cancels.

## Your feeds

**Adding:** press `Open-Apple-A` and type the feed's address. You can leave
off `https://`; newsiie adds it. Then it offers the site's name as the
feed's name: press `Return` to keep it, or erase it and type your own.

**Deleting:** choose the feed and press `Open-Apple-D`, then `Y`.

The list is kept in `NEWSIIE.FEEDS`, a plain text file next to newsiie,
with one feed a line: a name, a `|`, and the address. You can edit it in
any text editor, ZipEdit included:

```
Hacker News|https://hnrss.org/frontpage
https://xkcd.com/atom.xml
```

A line with only an address is named after its site. Lines starting with
`#` are notes for you; newsiie skips them, and does not keep them when it
saves the list. Up to 21 feeds fit.

## Read items

Opening an item marks it read. newsiie remembers the last 256 items you
read in `NEWSIIE.READ`, so they stay marked the next time you fetch the
feed. It saves that file when you go back to Feeds and when you quit.

## Saving an item

In an article, press `Open-Apple-S`. newsiie suggests a file name made from
the title, such as `JAMESWEBBTEL.MD`; press `Return`, or type another name
or a full ProDOS path such as `/MYDISK/NOTES/WEBB.MD`. It asks before
replacing a file. The file is Markdown: the title as a heading, the date
and the full link, then the summary's paragraphs, ready for ZipEdit.

## When something goes wrong

| The bar says | What it means |
|---|---|
| `NO COGS CARD` | newsiie did not find the card. Check it is seated, and that its slot is set to Your Card. |
| `WAITING FOR THE NETWORK` | The card has no network yet. newsiie waits up to 20 seconds; check the card's own configuration screen if it never connects. |
| `WAITING FOR THE CARD'S CLOCK` | Secure (https) feeds need the card to know the time, which it learns from the internet a few seconds after it connects. newsiie waits up to 20 seconds. |
| `DNS FAILED`, `TIMED OUT`, `CONNECTION REFUSED` | The feed's server could not be found or reached. Check the address. |
| `THE SERVER REFUSED IT (OR NOT FOUND)` | The server answered with an error, such as "not found". The feed may have moved. |
| `THAT IS NOT A FEED` | The address gives a web page, not RSS or Atom. Many sites link to their feed from their front page. |
| "memory full" in the heading | The feed is larger than newsiie can hold; the first items are shown. |
| `THE DISK IS WRITE-PROTECTED` | newsiie cannot save there. On a CoGS network disk, the image's server may not accept changes; the CoGS Image Manager's share does. Your feed list still works until you quit. |

## Booting from the CoGS network disk

newsiie is built to run this way. The card carries both the network disk
and the feeds, so newsiie never reads or writes the disk while a feed is
being fetched; it does its disk work before and after.

## Two test programs

The disk also carries two programs for checking the card, which you can
start from the ProDOS menu after quitting newsiie:

- `PROBE.SYSTEM` checks the card from end to end: slot, firmware, network,
  clock, and data in both directions.
- `FETCH.SYSTEM` fetches six test feeds and reports speed, size and what
  each one contained.
