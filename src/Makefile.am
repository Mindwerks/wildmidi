AUTOMAKE_OPTIONS=	foreign
INCLUDES=		-I$(top_srcdir)/include

lib_LTLIBRARIES=	libWildMidi.la
libWildMidi_la_SOURCES=		reverb.c wildmidi_lib.c
libWildMidi_la_CFLAGS=		$(PREFER_NON_PIC) 
libWildMidi_la_LIBADD=		-lm -lc
libWildMidi_la_LDFLAGS=		-shared $(LDFLAGS) -no-undefined

bin_PROGRAMS= wildmidi
wildmidi_SOURCES=		wildmidi.c 
wildmidi_LDADD=			libWildMidi.la
wildmidi_LGFLAGS=		$(AUDDR) $(LDFLAGS)
