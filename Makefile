# newsiie -- an RSS reader for the Enhanced Apple //e with a CoGS card.
#
#   make          build build/NEWSIIE.SYSTEM
#   make disk     build the bootable ProDOS image build/NEWSIIE.po
#   make run      build, boot it in Virtual ][, print the screen
#   make clean

TARGET  = apple2enh
CL65    = cl65
CFG     = cfg/newsiie.cfg
# -O optimise, -i inline, -r register vars, -s inline stdlib; -Cl static locals
COPT    = -Oirs -Cl

SRCS = \
	src/main.c \
	src/screen.c \
	src/kbd.c \
	src/splash.c \
	src/feeds.c \
	src/help.c \
	src/status.c

ASMS = \
	src/blit.s

OBJS = $(patsubst src/%.c,build/%.o,$(SRCS)) $(patsubst src/%.s,build/%.o,$(ASMS))
HDRS = $(wildcard src/*.h)

BIN  = build/NEWSIIE.SYSTEM
DISK = build/NEWSIIE.po

.PHONY: all disk run clean

all: $(BIN)

build:
	mkdir -p build

build/%.o: src/%.c $(HDRS) | build
	$(CL65) -t $(TARGET) $(COPT) -c -o $@ $<

build/%.o: src/%.s | build
	$(CL65) -t $(TARGET) -c -o $@ $<

$(BIN): $(OBJS) $(CFG)
	$(CL65) -t $(TARGET) -C $(CFG) -m build/newsiie.map -Ln build/newsiie.lbl -o $@ $(OBJS)
	@echo "NEWSIIE.SYSTEM: $$(($$(wc -c < $@) - 58)) bytes"

disk: $(DISK)

$(DISK): $(BIN) tools/mkdisk.sh
	tools/mkdisk.sh $(DISK) $(BIN)

run: $(DISK)
	tools/vii.sh boot $(DISK)
	tools/vii.sh await NEWSIIE 30
	tools/vii.sh screen

clean:
	rm -rf build
