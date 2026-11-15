#!/usr/bin/env python3
"""corpus_check.py FEEDDUMP CORPUS_DIR -- check newsiie's parser on real feeds.

For every feed in CORPUS_DIR, Python's own XML parser lists the item links
independently, and newsiie's parser (run through FEEDDUMP) must find exactly
the same links, in the same order. Its output must also be printable ASCII,
every item must have a title, and the dump must not change with the chunk
size the bytes arrive in. Real feeds change daily, so this checks
properties, not exact text; the fixtures do that.
"""
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ATOM = "{http://www.w3.org/2005/Atom}"
RSS1 = "{http://purl.org/rss/1.0/}"


def local(tag):
    return tag.rsplit("}", 1)[-1]


def reference_links(path):
    root = ET.parse(path).getroot()
    kind = local(root.tag)
    links = []
    if kind == "rss":
        for item in root.iter("item"):
            el = item.find("link")
            links.append((el.text or "").strip() if el is not None else "")
    elif kind == "feed":
        for entry in root.iter(ATOM + "entry"):
            href = ""
            for el in entry.findall(ATOM + "link"):
                if el.get("rel") in (None, "alternate"):
                    href = el.get("href", "")
                    break
            links.append(href)
    elif kind == "RDF":
        for item in root.iter(RSS1 + "item"):
            el = item.find(RSS1 + "link")
            links.append((el.text or "").strip() if el is not None else "")
    return kind, links


def dump(feeddump, path, chunk):
    out = subprocess.run([feeddump, "-c", str(chunk), str(path)],
                         capture_output=True, check=True).stdout
    return out


def main():
    feeddump, corpus = sys.argv[1], Path(sys.argv[2])
    feeds = sorted(corpus.glob("*.xml"))
    if not feeds:
        print(f"no feeds in {corpus}: run tools/getcorpus.sh")
        return 1
    failures = 0
    for path in feeds:
        problems = []
        kind, want = reference_links(path)
        out = dump(feeddump, path, 512)
        for chunk in (1, 7):
            if dump(feeddump, path, chunk) != out:
                problems.append(f"output differs at chunk size {chunk}")
        bad = [b for b in out if not (32 <= b < 127 or b == 10)]
        if bad:
            problems.append(f"{len(bad)} non-ASCII bytes in the output")
        text = out.decode("ascii", "replace").splitlines()
        got = [l[len("  link:  "):] for l in text if l.startswith("  link:  ")]
        titles = [l[len("  title: "):] for l in text if l.startswith("  title: ")]
        if got != want:
            problems.append(f"links differ: newsiie {len(got)}, Python {len(want)}")
            for i, (a, b) in enumerate(zip(got, want)):
                if a != b:
                    problems.append(f"  first difference at item {i + 1}: {a!r} vs {b!r}")
                    break
        if any(not t for t in titles):
            problems.append("an item with an empty title")
        status = "ok" if not problems else "FAIL"
        print(f"{path.name:14} {kind:5} {len(got):3} items  {status}")
        for p in problems:
            print("    " + p)
        failures += bool(problems)
    print(f"{len(feeds) - failures} of {len(feeds)} feeds pass")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
