#------------------------------------------------------------------------------#
# This is a Makefile for building libWildMidi using GNU Make and GNU C compiler
# You will need a GCC/DJGPP package and GNU make to do it painlessly.
#------------------------------------------------------------------------------#
# Targets:
# - all (default): make libWildMidi.a library and wildmidi.exe
# - clean: clean up (remove all generated files)
#------------------------------------------------------------------------------#

# Set to 1 for debug build
DEBUG = 0

# The tools
ifeq ($(CROSS),)
CC=gcc
AS=as
AR=ar
RANLIB=ranlib
else
CC=$(CROSS)-gcc
AS=$(CROSS)-as
AR=$(CROSS)-ar
RANLIB=$(CROSS)-ranlib
endif

INCLUDE = -I. -I../include/stdint -I../include
CFLAGS = $(INCLUDE) -Wall -W -fno-common
ARFLAGS = cr

# SB output support (comment out if not wanted)
CFLAGS+= -DAUDIODRV_DOSSB
SB_OBJ = dosirq.o dosdma.o dossb.o

LD = $(CC)
LDFLAGS = -L. -lWildMidi -lm

# build for pentium and newer (comment out if not wanted)
CFLAGS += -march=i586
ifeq ($(DEBUG),1)
CFLAGS += -g
else
CFLAGS += -O2 -fomit-frame-pointer -ffast-math
LDFLAGS+= -s
endif

# Build rules
%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
%.o: ../src/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

# Objects
LIB_OBJ= wm_error.o file_io.o lock.o wildmidi_lib.o reverb.o gus_pat.o f_xmidi.o f_mus.o f_hmp.o f_midi.o f_hmi.o mus2mid.o xmi2mid.o internal_midi.o patches.o sample.o
PLAYER_OBJ= $(SB_OBJ) getopt_long.o wm_tty.o wildmidi.o

# Build targets
TARGETS = libWildMidi.a wildmidi.exe

.PHONY: clean distclean
all: $(TARGETS)

libWildMidi.a: $(LIB_OBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

wildmidi.exe: libWildMidi.a $(PLAYER_OBJ)
	$(LD) -o $@ $(PLAYER_OBJ) $(LDFLAGS)

clean:
	rm -rf $(LIB_OBJ) $(PLAYER_OBJ)
distclean: clean
	rm -rf $(TARGETS)

