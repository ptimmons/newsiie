# newsiie -- an RSS reader for the Enhanced Apple //e with a CoGS card.
#
#   make            build NEWSIIE.SYSTEM and the two probes, PROBE and FETCH
#   make disk       the bootable ProDOS image build/NEWSIIE.po, carrying all three
#   make testdisk   build/NEWSIIE-TEST.po: the app with feeds from a disk too
#   make feedsdisk  build/FEEDS.po, that disk, from the corpus (not committed)
#   make run        build, boot it in Virtual ][, print the screen
#   make host-test  the C that has no Apple dependencies, run on the Mac:
#                   card protocol, cleaner, and the feed fixtures
#   make corpus     the parser on real feeds (tools/getcorpus.sh fetches them)
#   make clean

TARGET  = apple2enh
CL65    = cl65
CFG     = cfg/newsiie.cfg
# -Or: optimise with register variables, no inlining (measured 9% smaller than
# -Oirs, and the hot loops are assembly anyway); -Cl: static locals
COPT    = -Or -Cl

# Shared by the app and the probe.
COMMON = \
	src/screen.c \
	src/kbd.c \
	src/status.c \
	src/tick.c \
	src/fmt.c \
	src/cogs.c \
	src/slot.c \
	src/blit.s \
	src/cogs_io.s

# The feed parser. Plain C but for scan.s, whose twin on the Mac is
# scan_host.c.
PARSER = \
	src/feed.c \
	src/xml.c \
	src/clean.c \
	src/scan_tab.c \
	src/scan.s

# The app. feeds.c is built twice: the test build lists feed files on a
# second disk as well, so the emulator, which has no card, can drive it.
APP = \
	src/main.c \
	src/splash.c \
	src/help.c \
	src/net.c \
	src/ui.c \
	src/heads.c \
	src/store.c \
	src/source.c \
	src/auxmem.s \
	$(PARSER)

PROBE = \
	src/probe.c

FETCH = \
	src/fetch.c \
	src/disk.s \
	$(PARSER)

obj = $(patsubst src/%.s,build/%.o,$(patsubst src/%.c,build/%.o,$(1)))
HDRS = $(wildcard src/*.h)

BIN   = build/NEWSIIE.SYSTEM
TBIN  = build/test/NEWSIIE.SYSTEM
TDISK = build/NEWSIIE-TEST.po
PRBIN = build/PROBE.SYSTEM
FTBIN = build/FETCH.SYSTEM
DISK  = build/NEWSIIE.po

HOST_CC     = cc
HOST_CFLAGS = -std=c99 -Wall -Wextra -Werror -Isrc

.PHONY: all disk testdisk feedsdisk run host-test corpus clean

all: $(BIN) $(PRBIN) $(FTBIN)

build build/host build/test build/asm:
	mkdir -p $@

# C goes through assembly so tools/cc65check.py can look at it: this cc65
# has been seen to miscompile a pointer reload (see that script), and a
# recurrence must fail the build, not corrupt memory on the Apple.
build/%.o: src/%.c $(HDRS) tools/cc65check.py | build build/asm
	$(CL65) -t $(TARGET) $(COPT) -S -o build/asm/$*.s $<
	python3 -I tools/cc65check.py build/asm/$*.s > /dev/null || python3 -I tools/cc65check.py build/asm/$*.s
	$(CL65) -t $(TARGET) -c -o $@ build/asm/$*.s

build/%.o: src/%.s | build
	$(CL65) -t $(TARGET) -c -o $@ $<

$(BIN): $(call obj,$(APP) $(COMMON)) build/feeds.o $(CFG)
	$(CL65) -t $(TARGET) -C $(CFG) -m build/newsiie.map -Ln build/newsiie.lbl -o $@ $(call obj,$(APP) $(COMMON)) build/feeds.o
	@echo "NEWSIIE.SYSTEM: $$(($$(wc -c < $@) - 58)) bytes"

build/test/feeds.o: src/feeds.c $(HDRS) tools/cc65check.py | build/test
	$(CL65) -t $(TARGET) $(COPT) -DNEWSIIE_TEST -S -o build/test/feeds.s $<
	python3 -I tools/cc65check.py build/test/feeds.s > /dev/null || python3 -I tools/cc65check.py build/test/feeds.s
	$(CL65) -t $(TARGET) -c -o $@ build/test/feeds.s

$(TBIN): $(call obj,$(APP) $(COMMON)) build/test/feeds.o $(CFG)
	$(CL65) -t $(TARGET) -C $(CFG) -m build/test/newsiie.map -o $@ $(call obj,$(APP) $(COMMON)) build/test/feeds.o
	@echo "NEWSIIE.SYSTEM (test): $$(($$(wc -c < $@) - 58)) bytes"

$(PRBIN): $(call obj,$(PROBE) $(COMMON)) $(CFG)
	$(CL65) -t $(TARGET) -C $(CFG) -m build/probe.map -Ln build/probe.lbl -o $@ $(call obj,$(PROBE) $(COMMON))
	@echo "PROBE.SYSTEM: $$(($$(wc -c < $@) - 58)) bytes"

$(FTBIN): $(call obj,$(FETCH) $(COMMON)) $(CFG)
	$(CL65) -t $(TARGET) -C $(CFG) -m build/fetch.map -Ln build/fetch.lbl -o $@ $(call obj,$(FETCH) $(COMMON))
	@echo "FETCH.SYSTEM: $$(($$(wc -c < $@) - 58)) bytes"

disk: $(DISK)

# NEWSIIE.SYSTEM goes first, so it is the one ProDOS launches at boot.
# The probes are launched from the quit selector.
$(DISK): $(BIN) $(PRBIN) $(FTBIN) tools/mkdisk.sh
	tools/mkdisk.sh $(DISK) $(BIN) $(PRBIN) $(FTBIN)

testdisk: $(TDISK)

$(TDISK): $(TBIN) $(PRBIN) $(FTBIN) tools/mkdisk.sh
	tools/mkdisk.sh $(TDISK) $(TBIN) $(PRBIN) $(FTBIN)

# Four corpus feeds on a 140K disk named /FEEDS, for drive 2.
feedsdisk: build/FEEDS.po

build/FEEDS.po: tools/mkfeeds.sh
	@[ -d build/corpus ] || tools/getcorpus.sh
	tools/mkfeeds.sh $@

run: $(DISK)
	tools/vii.sh boot $(DISK)
	tools/vii.sh await NEWSIIE 30
	tools/vii.sh screen

build/host/test_cogs: tests/host/test_cogs.c src/cogs.c src/fmt.c $(HDRS) | build/host
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/host/test_cogs.c src/cogs.c src/fmt.c

HOST_PARSER = src/feed.c src/xml.c src/clean.c src/scan_host.c src/scan_tab.c

build/host/test_clean: tests/host/test_clean.c src/clean.c $(HDRS) | build/host
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/host/test_clean.c src/clean.c

build/host/feeddump: tests/host/feeddump.c $(HOST_PARSER) $(HDRS) | build/host
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/host/feeddump.c $(HOST_PARSER)

# Each fixture must parse to exactly its .out, whether the bytes arrive one
# at a time, seven at a time, or 512 at a time.
FIXTURES = $(wildcard tests/fixtures/*.xml)

build/host/test_store: tests/host/test_store.c src/store.c $(HDRS) | build/host
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/host/test_store.c src/store.c

host-test: build/host/test_cogs build/host/test_clean build/host/test_store build/host/feeddump
	build/host/test_cogs
	build/host/test_clean
	build/host/test_store
	@fail=0; for f in $(FIXTURES); do \
	    for c in 1 7 512; do \
	        if ! build/host/feeddump -c $$c $$f | diff -u $${f%.xml}.out - ; then \
	            echo "FIXTURE FAILED: $$f at chunk size $$c"; fail=1; \
	        fi; \
	    done; \
	done; \
	[ $$fail = 0 ] && echo "all $(words $(FIXTURES)) fixtures match at chunk sizes 1, 7 and 512"; \
	exit $$fail

corpus: build/host/feeddump
	@[ -d build/corpus ] || tools/getcorpus.sh
	python3 -I tools/corpus_check.py build/host/feeddump build/corpus

clean:
	rm -rf build
