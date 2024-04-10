# Makefile for Win32 using Visual Studio 2010 / newer:
#	nmake -f Makefile.vs

INCLUDES  = -I. -I..\include
CPPFLAGS  = -DNDEBUG -D_CRT_SECURE_NO_WARNINGS

CFLAGS    = -nologo /O2 /MD /W3
LDFLAGS   = -nologo /SUBSYSTEM:CONSOLE

DLL_FLAGS = $(CFLAGS) $(CPPFLAGS) -DWILDMIDI_BUILD -DDLL_EXPORT
PLY_FLAGS = $(CFLAGS) $(CPPFLAGS)

!INCLUDE common.mak
