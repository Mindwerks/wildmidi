AUTOMAKE_OPTIONS=	foreign
INCLUDES=		-I$(top_srcdir)/include

lib_LTLIBRARIES=	libWildMidi.la
libWildMidi_la_SOURCES=		wm_error.c file_io.c reverb.c wildmidi_lib.c
libWildMidi_la_CFLAGS=		$(PREFER_NON_PIC)
libWildMidi_la_LDFLAGS=		-shared -lm -lc $(LDFLAGS) -no-undefined -export-symbols-regex '^WildMidi.*'

bin_PROGRAMS= wildmidi
wildmidi_libs=			./libWildMidi.la
wildmidi_SOURCES=		wildmidi.c
wildmidi_LDADD=			$(AUDDR) $(LDFLAGS) -L./ -lWildMidi
wildmidi_DEPENDENCIES=	./libWildMidi.la
