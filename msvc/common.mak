# common stuff for MSVC Nmakefiles.
#
CC      = cl
LD      = link
RC      = rc

LIBNAME = libWildMidi
DLLNAME = $(LIBNAME).dll
IMPNAME = $(LIBNAME).lib
PLAYER  = wildmidi.exe
LIBS_DLL=
LIBS_PLY= $(IMPNAME) winmm.lib

DLL_OBJ = wm_error.obj file_io.obj lock.obj wildmidi_lib.obj reverb.obj gus_pat.obj f_xmidi.obj f_mus.obj f_hmp.obj f_midi.obj f_hmi.obj mus2mid.obj xmi2mid.obj internal_midi.obj patches.obj sample.obj
PLY_OBJ = wm_tty.obj msleep.obj getopt_long.obj out_none.obj out_wave.obj out_win32mm.obj wildmidi.obj
# out_openal.obj

all: $(DLLNAME) $(PLAYER)

$(DLLNAME): $(DLL_OBJ)
	$(LD) -out:$@ -dll $(LDFLAGS) $(DLL_OBJ) $(LIBS_DLL)
$(PLAYER): $(DLLNAME) $(PLY_OBJ)
	$(LD) -out:$@ $(LDFLAGS) $(PLY_OBJ) $(LIBS_PLY)

# dll objects:
wm_error.obj: ..\src\wm_error.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
file_io.obj: ..\src\file_io.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
lock.obj: ..\src\lock.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
wildmidi_lib.obj: ..\src\wildmidi_lib.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
reverb.obj: ..\src\reverb.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
gus_pat.obj: ..\src\gus_pat.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
f_xmidi.obj: ..\src\f_xmidi.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
f_mus.obj: ..\src\f_mus.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
f_hmp.obj: ..\src\f_hmp.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
f_midi.obj: ..\src\f_midi.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
f_hmi.obj: ..\src\f_hmi.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
mus2mid.obj: ..\src\mus2mid.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
xmi2mid.obj: ..\src\xmi2mid.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
internal_midi.obj: ..\src\internal_midi.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
patches.obj: ..\src\patches.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?
sample.obj: ..\src\sample.c
	$(CC) $(DLL_FLAGS) $(INCLUDES) -c -Fo$@ $?

# player objects:
wildmidi.obj: ..\src\player\wildmidi.c
	$(CC) $(PLY_FLAGS) $(INCLUDES) -c -Fo$@ $?
out_none.obj: ..\src\player\out_none.c
	$(CC) $(PLY_FLAGS) $(INCLUDES) -c -Fo$@ $?
out_wave.obj: ..\src\player\out_wave.c
	$(CC) $(PLY_FLAGS) $(INCLUDES) -c -Fo$@ $?
out_win32mm.obj: ..\src\player\out_win32mm.c
	$(CC) $(PLY_FLAGS) $(INCLUDES) -c -Fo$@ $?
out_openal.obj: ..\src\player\out_openal.c
	$(CC) $(PLY_FLAGS) $(INCLUDES) -c -Fo$@ $?
msleep.obj: ..\src\player\msleep.c
	$(CC) $(PLY_FLAGS) $(INCLUDES) -c -Fo$@ $?
wm_tty.obj: ..\src\player\wm_tty.c
	$(CC) $(PLY_FLAGS) $(INCLUDES) -c -Fo$@ $?
getopt_long.obj: ..\src\getopt_long.c
	$(CC) $(PLY_FLAGS) $(INCLUDES) -c -Fo$@ $?

distclean: clean
	-del $(DLLNAME) $(IMPNAME) $(PLAYER)
clean:
	-del *.obj *.exp *.manifest
