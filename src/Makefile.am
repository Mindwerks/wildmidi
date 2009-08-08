AM_CPPFLAGS=		-I$(top_srcdir)/include

lib_LTLIBRARIES=	libWildMidi.la
libWildMidi_la_SOURCES=		lock.c error.c file_io.c reverb.c wildmidi_lib.c
libWildMidi_la_CFLAGS=		$(PREFER_NON_PIC) 
libWildMidi_la_LIBADD=		-lm -lc $(LIBADD)
libWildMidi_la_LDFLAGS=		-shared $(LDFLAGS) -no-undefined

bin_PROGRAMS= wildmidi
wildmidi_SOURCES=		wildmidi.c 
wildmidi_LDADD=			$(LDADD) -L./ -lWildMidi
wildmidi_LDFLAGS=		$(LDFLAGS)
