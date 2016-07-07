# GNU Makefile to build for AmigaOS variants using gcc
#
# make aros     : build for i386-aros
# make morphos  : build for ppc-morphos
# make amigaos  : build for m68k-amigaos
# make amigaos4 : build for ppc-amigaos4

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

INCLUDE = -I. -I../include
CFLAGS = $(INCLUDE) -Wall -W -fno-common
ARFLAGS = cr
LD = $(CC)
LDFLAGS = -L. -lWildMidi

ifeq ($(DEBUG),1)
CFLAGS += -g
else
CFLAGS += -O2
endif

ifeq ($(AROS),1)
include gcc_aros.cfg
endif
ifeq ($(MORPHOS),1)
include gcc_morphos.cfg
endif
ifeq ($(AOS3),1)
include gcc_aos3.cfg
endif
ifeq ($(AOS4),1)
include gcc_aos4.cfg
endif

# Build rules
%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
%.o: ../src/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

# Objects
LIB_OBJ= wm_error.o file_io.o lock.o wildmidi_lib.o reverb.o gus_pat.o f_xmidi.o f_mus.o f_hmp.o f_midi.o f_hmi.o mus2mid.o xmi2mid.o internal_midi.o patches.o sample.o
PLAYER_OBJ= getopt_long.o wm_tty.o amiga.o wildmidi.o

# Build targets
.PHONY: clean distclean
all: libWildMidi.a wildmidi

libWildMidi.a: $(LIB_OBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

wildmidi: libWildMidi.a $(PLAYER_OBJ)
	$(LD) -o $@ $(PLAYER_OBJ) $(LDFLAGS)

aros:
	@echo Building for i386-aros
	$(MAKE) AROS=1
morphos:
	@echo Building for ppc-morphos
	$(MAKE) MORPHOS=1
amigaos:
	@echo Building for m68k-amigaos
	$(MAKE) AOS3=1
amigaos4:
	@echo Building for ppc-amigaos4
	$(MAKE) AOS4=1

clean:
	rm -rf $(LIB_OBJ) $(PLAYER_OBJ)
distclean: clean
	rm -rf libWildMidi.a WildMidi.lib wildmidi
