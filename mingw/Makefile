# Makefile to build WildMidi library and player for Windows using MinGW / MinGW-w64.

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
LD = $(CC)

LIBNAME=WildMidi
LIBSTATIC=lib$(LIBNAME).a
LIBSHARED=lib$(LIBNAME).dll
DEFNAME=lib$(LIBNAME).def
IMPLIB=lib$(LIBNAME).dll.a
PLAYER=wildmidi.exe
PLAYER_STATIC=wildmidi-static.exe

INCLUDE=-I. -I../include
ARFLAGS=cr
CFLAGS=$(INCLUDE) -Wall -W -fno-common
LDFLAGS=
LDLIBS=
ifeq ($(DEBUG),1)
CFLAGS+=-g
else
CFLAGS+=-O2 -fomit-frame-pointer -ffast-math
endif

CFLAGS_LIB=$(CFLAGS) -DWILDMIDI_BUILD
LDFLAGS_LIB=$(LDFLAGS) -shared -mconsole
LDLIBS_LIB=$(LDLIBS)
LDFLAGS_LIB+=-Wl,--enable-auto-image-base -Wl,--no-undefined \
		-Xlinker --output-def -Xlinker $(DEFNAME) \
		-Xlinker --out-implib -Xlinker $(IMPLIB)

CFLAGS_EXE=$(CFLAGS)
LDFLAGS_EXE=$(LDFLAGS) -mconsole
LDLIBS_EXE=$(LDLIBS)
# Standard WinMM output support:
LDLIBS_EXE+=-lwinmm
# OpenAL output support (if really wanted)
#CFLAGS_EXE+=-DAUDIODRV_OPENAL
#LDFLAGS_EXE+=-lOpenAL32
LDLIBS_EXE+=-L. -l$(LIBNAME)

# Objects
LIB_OBJ = wm_error.o file_io.o lock.o wildmidi_lib.o reverb.o gus_pat.o
LIB_OBJ+= f_xmidi.o f_mus.o f_hmp.o f_midi.o f_hmi.o mus2mid.o xmi2mid.o internal_midi.o patches.o sample.o
PLAYER_OBJ = wm_tty.o getopt_long.o wildmidi.o

.PHONY: clean distclean

help:
	@echo "Valid targets:"
	@echo ""
	@echo "  $(MAKE) shared"
	@echo "  $(MAKE) static"
	@echo ""

shared: $(LIBSHARED) $(PLAYER)
shared: CFLAGS_LIB+=-DDLL_EXPORT

static: $(LIBSTATIC) $(PLAYER_STATIC)
static: CFLAGS+=-DWILDMIDI_STATIC

$(LIBSHARED): $(LIB_OBJ)
	$(CC) $^ $(LDFLAGS_LIB) $(LDLIBS_LIB) -o $@

$(LIBSTATIC): $(LIB_OBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

$(PLAYER): $(PLAYER_OBJ) $(LIBSHARED)
	$(LD) $(PLAYER_OBJ) $(LDFLAGS_EXE) $(LDLIBS_EXE) -o $@

$(PLAYER_STATIC): $(PLAYER_OBJ) $(LIBSTATIC)
	$(LD) $(PLAYER_OBJ) $(LDFLAGS_EXE) $(LDLIBS_EXE) -o $@

# Build rules
%.o: %.c
	$(CC) -c $(CFLAGS_LIB) -o $@ $<
%.o: ../src/%.c
	$(CC) -c $(CFLAGS_LIB) -o $@ $<
# for player objects:
wildmidi.o: ../src/wildmidi.c
	$(CC) -c $(CFLAGS_EXE) -o $@ $<
wm_tty.o: ../src/wm_tty.c
	$(CC) -c $(CFLAGS_EXE) -o $@ $<
getopt_long.o: ../src/getopt_long.c
	$(CC) -c $(CFLAGS_EXE) -o $@ $<

clean:
	rm -rf $(LIB_OBJ) $(PLAYER_OBJ)

distclean: clean
	rm -rf *.a *.dll *.def *.exe
