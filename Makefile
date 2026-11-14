# newsiie -- an RSS reader for the Enhanced Apple //e with a CoGS card.
#
#   make            build build/NEWSIIE.SYSTEM and build/PROBE.SYSTEM
#   make disk       the bootable ProDOS image build/NEWSIIE.po, carrying both
#   make run        build, boot it in Virtual ][, print the screen
#   make host-test  the C that has no Apple dependencies, run on the Mac
#   make clean

TARGET  = apple2enh
CL65    = cl65
CFG     = cfg/newsiie.cfg
# -O optimise, -i inline, -r register vars, -s inline stdlib; -Cl static locals
COPT    = -Oirs -Cl

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

APP = \
	src/main.c \
	src/splash.c \
	src/feeds.c \
	src/help.c \
	src/net.c

PROBE = \
	src/probe.c

obj = $(patsubst src/%.s,build/%.o,$(patsubst src/%.c,build/%.o,$(1)))
HDRS = $(wildcard src/*.h)

BIN   = build/NEWSIIE.SYSTEM
PRBIN = build/PROBE.SYSTEM
DISK  = build/NEWSIIE.po

HOST_CC     = cc
HOST_CFLAGS = -std=c99 -Wall -Wextra -Werror -Isrc

.PHONY: all disk run host-test clean

all: $(BIN) $(PRBIN)

build build/host:
	mkdir -p $@

build/%.o: src/%.c $(HDRS) | build
	$(CL65) -t $(TARGET) $(COPT) -c -o $@ $<

build/%.o: src/%.s | build
	$(CL65) -t $(TARGET) -c -o $@ $<

$(BIN): $(call obj,$(APP) $(COMMON)) $(CFG)
	$(CL65) -t $(TARGET) -C $(CFG) -m build/newsiie.map -Ln build/newsiie.lbl -o $@ $(call obj,$(APP) $(COMMON))
	@echo "NEWSIIE.SYSTEM: $$(($$(wc -c < $@) - 58)) bytes"

$(PRBIN): $(call obj,$(PROBE) $(COMMON)) $(CFG)
	$(CL65) -t $(TARGET) -C $(CFG) -m build/probe.map -Ln build/probe.lbl -o $@ $(call obj,$(PROBE) $(COMMON))
	@echo "PROBE.SYSTEM: $$(($$(wc -c < $@) - 58)) bytes"

disk: $(DISK)

# NEWSIIE.SYSTEM goes first, so it is the one ProDOS launches at boot.
# PROBE.SYSTEM is launched from the quit selector.
$(DISK): $(BIN) $(PRBIN) tools/mkdisk.sh
	tools/mkdisk.sh $(DISK) $(BIN) $(PRBIN)

run: $(DISK)
	tools/vii.sh boot $(DISK)
	tools/vii.sh await NEWSIIE 30
	tools/vii.sh screen

build/host/test_cogs: tests/host/test_cogs.c src/cogs.c src/fmt.c $(HDRS) | build/host
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/host/test_cogs.c src/cogs.c src/fmt.c

host-test: build/host/test_cogs
	build/host/test_cogs

clean:
	rm -rf build
