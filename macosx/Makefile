# Makefile to build WildMidi library and player for Mac OS X.

# Set to 1 for debug build
DEBUG = 0

# The tools
ifeq ($(CROSS),)
CC=gcc
AS=as
AR=ar
RANLIB=ranlib
LIPO=lipo
else
CC=$(CROSS)-gcc
AS=$(CROSS)-as
AR=$(CROSS)-ar
RANLIB=$(CROSS)-ranlib
LIPO=$(CROSS)-lipo
endif
LD = $(CC)

DYLIB_MAJ=2
DYLIB_MID=1
DYLIB_MIN=0

LIBNAME=WildMidi
LIBSTATIC=lib$(LIBNAME).a
LIBSHARED=lib$(LIBNAME).dylib
LIBSHARED_FULL=lib$(LIBNAME).$(DYLIB_MAJ).$(DYLIB_MID).$(DYLIB_MIN).dylib
LIBSHARED_MAJ=lib$(LIBNAME).$(DYLIB_MAJ).dylib
LIB_INSTALLNAME=$(LIBSHARED_MAJ)
PLAYER=wildmidi
PLAYER_STATIC=wildmidi-static

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
# specifically for 10.5 and newer:
#CFLAGS+=-mmacosx-version-min=10.5
#LDFLAGS+=-mmacosx-version-min=10.5
# specifically for 10.6 and newer:
#CFLAGS+=-mmacosx-version-min=10.6
#LDFLAGS+=-mmacosx-version-min=10.6 -Wl,-lbundle1.o

CFLAGS_LIB=$(CFLAGS) -DWILDMIDI_BUILD
LDFLAGS_LIB=$(LDFLAGS)
LDLIBS_LIB=$(LDLIBS)
LDFLAGS_LIB+=-dynamiclib -Wl,-single_module
LDFLAGS_LIB+=-Wl,-undefined,error
LDFLAGS_LIB+=-Wl,-install_name,"@executable_path/$(LIB_INSTALLNAME)"
LDFLAGS_LIB+=-Wl,-compatibility_version,$(DYLIB_MAJ).0.0 -Wl,-current_version,$(DYLIB_MAJ).$(DYLIB_MID).$(DYLIB_MIN)

CFLAGS_EXE=$(CFLAGS)
LDFLAGS_EXE=$(LDFLAGS)
LDLIBS_EXE=$(LDLIBS)
# OpenAL output support (comment out if not wanted)
CFLAGS_EXE+=-DAUDIODRV_OPENAL
LDFLAGS_EXE+=-Wl,-framework,OpenAL
LDLIBS_EXE+=-L. -l$(LIBNAME)

# Objects
LIB_OBJ = wm_error.o file_io.o lock.o wildmidi_lib.o reverb.o gus_pat.o
LIB_OBJ+= f_xmidi.o f_mus.o f_hmp.o f_midi.o f_hmi.o mus2mid.o xmi2mid.o internal_midi.o patches.o sample.o
PLAYER_OBJ = wm_tty.o wildmidi.o

.PHONY: clean distclean

help:
	@echo "Valid targets:"
	@echo ""
	@echo "  $(MAKE) shared"
	@echo "  $(MAKE) static"
	@echo ""

shared: $(LIBSHARED) $(PLAYER)
shared: CFLAGS_LIB+=-fvisibility=hidden -DSYM_VISIBILITY

static: $(LIBSTATIC) $(PLAYER_STATIC)
static: CFLAGS+=-DWILDMIDI_STATIC

$(LIBSHARED_FULL): $(LIB_OBJ)
	$(CC) $^ $(LDFLAGS_LIB) $(LDLIBS_LIB) -o $@
$(LIBSHARED_MAJ): $(LIBSHARED_FULL)
	ln -s $^ $@
$(LIBSHARED): $(LIBSHARED_MAJ)
	ln -s $^ $@

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

clean:
	rm -rf $(LIB_OBJ) $(PLAYER_OBJ)

distclean: clean
	rm -rf *.a *.dylib $(PLAYER) $(PLAYER_STATIC)
