AUTOMAKE_OPTIONS=	foreign
INCLUDES=		-I$(top_srcdir)
SUBDIRS=		include src docs
EXTRA_DIST= COPYING changelog configure.ac

changelog::
	( cd $(top_srcdir); \
		./cvs2cl.pl --stdout -b --utc ) > ChangeLog

