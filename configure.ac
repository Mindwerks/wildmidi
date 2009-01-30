dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.52)
AC_INIT(include/wildmidi_lib.h)
AC_REVISION($Revision: 1.13 $) dnl
AM_CONFIG_HEADER(include/config.h)
AC_CANONICAL_SYSTEM
AM_MAINTAINER_MODE
AC_VALIDATE_CACHED_SYSTEM_TUPLE(
	rm $cache_file
	echo restart configure
	exit 1
)
AM_INIT_AUTOMAKE(wildmidi, 0.2.3)
PROGRAM=WildMidi
WILDMIDI_VERSION=0.2.3-devel
WILDMIDILIB_VERSION=0.2.3-devel

AC_DEFINE_UNQUOTED(PROGRAM,             "$PROGRAM",["Proper" program name])
AC_DEFINE_UNQUOTED(WILDMIDI_VERSION,    "$WILDMIDI_VERSION", [current program version])
AC_DEFINE_UNQUOTED(WILDMIDILIB_VERSION,    "$WILDMIDILIB_VERSION", [current lib version])
 
AC_SUBST(PROGRAM)
AC_SUBST(WILDMIDI_VERSION)
AC_SUBST(WILDMIDILIB_VERSION)

ISODATE=`date +%Y-%m-%d`
AC_SUBST(ISODATE)

AC_LANG_C

if test ${CFLAGS-unset} = unset; then
	CFLAGS=""
fi
if test ${AR-unset} = unset; then
	AR="ar"
fi
export AR

AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LN_S
AC_PROG_RANLIB
AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL


set $CC
if test "$1" = gcc; then
	shift
	args="$*"
	AC_MSG_CHECKING(for gcc version)
	CCVER="gcc `gcc --version`"
	AC_MSG_RESULT($CCVER)
fi

case "$host_os" in
	mingw32*)
		mingw=yes
		CFLAGS="$CFLAGS -I\$(top_srcdir)/include/win32"
		LIBADD="$LIBADD -luser32 -lgdi32"
		if test "x$host" != "x$build"; then
			case "$build_os" in
				cygwin*)
					CFLAGS="$CFLAGS -mno-cygwin -mconsole"
					LIBADD="-L/usr/lib/w32api $LIBADD"
					CPPFLAGS="$CPPFLAGS $CFLAGS"
					;;
				*)
					CC="$host_cpu-$host_os-gcc"
					AS="$CC"
					AR="$host_cpu-$host_os-ar"
					RANLIB="$host_cpu-$host_os-ranlib"
					;;
			esac
		fi
		endian="little"
		;;
	cygwin*)
		cygwin=yes
		if test "x$host" != "x$build"; then
			CC="$host_cpu-$host_os-gcc"
			AS="$CC"
			AR="$host_cpu_$host_os-ar"
			RANLIB="$host_cpu-$host_os-ranlib"
			endian="little"
		fi
	;;
esac

AC_CHECK_HEADERS(
	alsa/asoundlib.h ctype.h errno.h fcntl.h getopt.h linux/soundcard.h machine/soundcard.h math.h mmsystem.h pwd.h \
      stdarg.h stdio.h stdlib.h string.h sys/ioctl.h sys/mman.h sys/soundcard.h sys/stat.h sys/types.h \
      termios.h unistd.h windows.h
)

AC_C_BIGENDIAN

CFLAGS="$CFLAGS -Wall"
AC_ARG_ENABLE(werror,
    [  --disable-werror      disable fail on compile warnings],    werror=$enable_werror,
	werror=yes
)

AC_MSG_CHECKING(for Werror)
if test "x$werror" = xyes; then
	AC_MSG_RESULT(yes)
	CFLAGS="$CFLAGS -Werror"
else 
	AC_MSG_RESULT(no)
fi

LDFLAGS=

AC_ARG_ENABLE(optimize,
    [  --disable-optimize      compile without optimizations (for development)],    optimize=$enable_optimize,
	optimize=no
)
AC_MSG_CHECKING(for optimization)
if test "x$optimize" = xyes; then
    AC_MSG_RESULT(yes)
	if test "x$GCC" = xyes; then
		BUILD_TYPE="$BUILD_TYPE Optimized"
		CFLAGS="$CFLAGS -O2 -std=gnu99 -frename-registers -finline-limit=32000 -Winline -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations -fstrict-aliasing -pipe -Wall -Werror -Wwrite-strings -fno-common"
		dnl CFLAGS="$CFLAGS -O2 -std=gnu99 -frename-registers -finline-limit=32000 -Winline -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -fstrict-aliasing -pipe -Wall -Werror -fno-common"
	else
		CFLAGS="$CFLAGS -O2"
	fi
else
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING(for __builtin_expect)
AC_TRY_COMPILE(
	[long (*foo) (long, long) = __builtin_expect;],
	[],
	AC_DEFINE(HAVE___BUILTIN_EXPECT)
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

AH_VERBATIM([HAVE___BUILTIN_EXPECT],
[/* Define this if the GCC __builtin_expect keyword is available */
#undef HAVE___BUILTIN_EXPECT
#ifndef HAVE___BUILTIN_EXPECT
# define __builtin_expect(x,c) x
#endif])

dnl CFLAGS for release and devel versions
AC_ARG_WITH(efence,
        [  --with-efence           link with -lefence],
		AC_CHECK_LIB(efence,malloc,LDFLAGS="$LDFLAGS -lefence")
)
		

dnl CFLAGS for release and devel versions
AC_ARG_ENABLE(debug,
    [  --enable-debug         compile with debugging],
	debug=$enable_debug, debug=yes 
)
AC_MSG_CHECKING(for debug)
if test "x$debug" = xyes; then
	AC_MSG_RESULT(yes)
	BUILD_TYPE="$BUILD_TYPE Debug"
	CFLAGS="$CFLAGS -g"
else 
	AC_MSG_RESULT(no)
fi

AC_ARG_ENABLE(profile,
	[  --enable-profile        compile with profiling (for development)],
	profile=$enable_profile
)
if test "x$profile" = xyes; then
	BUILD_TYPE="$BUILD_TYPE Profile"
	CFLAGS="`echo $CFLAGS | sed -e 's/-fomit-frame-pointer//g'` -pg -fprofile-arcs"
	LDFLAGS="$LDFLAGS -pg -fprofile-arcs"
fi

AC_ARG_ENABLE(temps,
	[  --enable-temps        compile with save-temps],
	temps=$enable_temps
)
if test "x$temps" = xyes; then
	CFLAGS="$CFLAGS -save-temps"
fi

AC_ARG_WITH(amd,
    [  --with-amd              Optimize for AMD processors instead of Intel],
	HAVE_AMD="yes",
	HAVE_AMD="no"
)

dnl Add more targets here if required.

case "$host_os" in
	*mingw*)
		default_timidityconf="timidity.cfg"
		;;
	*freebsd*)
		default_timifityconf="/usr/local/share/timidity/timidity.cfg"
		;;
	*)
		default_timidityconf="/etc/timidity.cfg"
		;;
esac

AC_ARG_WITH(timidity-cfg,
[  --with-timidity-cfg=FILE  If set will change the name and location of the
                          timidity config file used by WildMidi.  Defaults to
                          ./timidity.cfg - win32 mingw
                          /usr/local/share/timidity/timidity.cfg - FreeBSD
                          /etc/timidity.cfg - everything else],
timidityconf="$withval", timidityconf="auto")
if test "x$timidityconf" = "xauto" || test "x$timidityconf" = "xyes" || \
	test "x$timidityconf" = "xno"; then  dnl yes/no sanity checks
	timidityconf="$default_timidityconf"
fi

AC_DEFINE_UNQUOTED(TIMIDITY_CFG, "$timidityconf", [Define this to the location of the timidity config file])

AC_MSG_CHECKING(for special compiler settings)

AC_ARG_WITH(arch,
	[  --with-arch=ARCH        control compiler architecture directly],
	arch="$withval", arch=auto
)

case "$arch" in
	no)
		;;
	auto)
		case "${host_cpu}" in
			i?86)
				if test "x$HAVE_AMD" = "xyes"; then
					CFLAGS="$CFLAGS -march=k6"
				else
					CFLAGS="$CFLAGS -march=${host_cpu}"
				fi
				;;
		esac
		;;
	*)
		CFLAGS="$CFLAGS -march=$arch"
		;;
									
esac

case "$host_os" in
        *mingw*)
		AUDLIB="-lwinmm"
        ;;
        *)
		USE_ALSA=no
		USE_OSS=no
		AC_CHECK_HEADERS(alsa/asoundlib.h)
		if test "x$ac_cv_header_alsa_asoundlib_h" = "xyes"; then
			AC_CHECK_LIB(asound,snd_pcm_mmap_commit,USE_ALSA=yes)
                        AC_ARG_WITH(oss,[  --with-oss              use oss instead of alsa (not required on systems]
					[          that don't have alsa sound drivers installed)], USE_OSS=yes)

			if test "x$USE_ALSA" = xyes ; then
				if test "x$USE_OSS" = xno ; then 
					AUDLIB="-lasound"
					AC_DEFINE(HAVE_ALSA, 1, [Define this if your system uses alsa])
					AC_TRY_COMPILE(
						[#include <alsa/asoundlib.h>],
						[void f(void) {
							snd_pcm_t *pcm;
							int rate = 44100;
							snd_pcm_hw_params_t *hw;
							int err;
							err = snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
						}],
						AC_DEFINE(ALSA_NEW, 1, [Define this if using Alsa 1.0.1 or higher])
					)
				fi
			fi
		fi
       ;;
esac

AC_SUBST(HAVE_ALSA)
AC_SUBST(CFLAGS)
AC_SUBST(LIBADD)
AC_SUBST(LDFLAGS)
AC_SUBST(AUDLIB)
AC_SUBST(AS)
AC_SUBST(AR)
AC_SUBST(RANLIB)

AC_OUTPUT(
 Makefile
 src/Makefile
 include/Makefile

)
if test -d CVS; then
	echo " "
	echo "WARNING: Developement source, expect problems."
fi
AC_MSG_RESULT([
	You may now type make to build WildMidi 
])
