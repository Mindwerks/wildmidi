#------------------------------------------------------------------------------#
# Makefile for building WildMidi library and player for DOS.
# You will need a DJGPP build environment and GNU make to do it painlessly.
# To build natively on DOS, you need long file names support (e.g. doslfn.)
#------------------------------------------------------------------------------#

# Set to 1 for debug build
DEBUG = 0

# DXE3 version of the library requires DJGPP-2.05 or newer.
# Set to 0 if dont want to build / use a DXE3 version of the library.
USE_DXE = 1

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
DXE3GEN=dxe3gen

INCLUDE = -I. -I../include/stdint -I../include
CFLAGS = $(INCLUDE) -Wall -W -fno-common
ARFLAGS = cr

# SB output support (comment out if not wanted)
CFLAGS+= -DAUDIODRV_DOSSB
SB_OBJ = dosirq.o dosdma.o dossb.o

LD = $(CC)
ifneq ($(USE_DXE),1)
WM_LINK = WildMidi
else
WM_LINK = WildMidi_dxe
endif
LDFLAGS = -L. -l$(WM_LINK)

# build for pentium and newer (comment out if not wanted)
CFLAGS += -march=i586
ifeq ($(DEBUG),1)
CFLAGS += -g
else
CFLAGS += -O2 -fomit-frame-pointer -ffast-math
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
TARGETS = libWildMidi.a wildmidi.exe libWildMidi_dxe.a wildmidi.dxe

.PHONY: clean distclean
all: $(TARGETS)

libWildMidi.a: $(LIB_OBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

libWildMidi_dxe.a: wildmidi.dxe
wildmidi.dxe: $(LIB_OBJ)
	-$(DXE3GEN) -Y libWildMidi_dxe.a -o $@ -E _WildMidi_ -U  $^

wildmidi.exe: lib$(WM_LINK).a $(PLAYER_OBJ)
	$(LD) -o $@ $(PLAYER_OBJ) $(LDFLAGS)

clean:
	rm -rf $(LIB_OBJ) $(PLAYER_OBJ)
distclean: clean
	rm -rf $(TARGETS)

