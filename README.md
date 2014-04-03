WildMIDI is a simple software midi player which has a core softsynth
library that can be use with other applications.

The WildMIDI library uses Gravis Ultrasound patch files to convert MIDI
files into audio which is then passed back to the calling application.
The library API is designed so that it is easy to include WildMIDI into
applications that wish to include MIDI file playback.

Version: 0.3.6
Licenses: GPLv3+ and LGPLv3
Website: http://www.mindwerks.net/projects/wildmidi

PLATFORMS:

* Linux: Arch, Debian, Fedora, Ubuntu
* Windows: x32 and x64
* BSD: Debian, FreeBSD, NetBSD, OpenBSD
* Hurd: Debian

BUILD FROM SOURCE:

Requirements:
* git
* cmake
* GCC / Xcode / VisualStudio / MinGW

CHANGELOG

0.3.6
* Fix some portability issues.
* Fix a double-free issue during library shutdown when several midis
  were alive.
* Fix the invalid option checking in WildMidi_Init().
* Fix the roundtempo option which had been broken since its invention
  in 0.2.3.5 (WM_MO_ROUNDTEMPO: was 0xA000 instead of 0x2000.)
* Fix cfg files without a newline at the end weren't parsed correctly.
* Handle cfg files with mac line-endings.
* Refuse loading suspiciously long files.

0.3.5
* Greatly reduced the heap usage (was a regression introduced in 0.2.3)
* OpenAL support: Fixed audio output on big-endian systems. Fixed audio
  skips at song start.
* OSS support: No longer uses mmap mode for better compatibility. This
  gains us NetBSD and OpenBSD support.
* Worked around an invalid memory read found by valgrind when playing
  Beethoven's Fur Elise.rmi at 44100 Hz using the old MIDIA patch-set
  from 1994.
* Build fixes for MSVC. Revised visibility attributes usage.

0.3.4
* OpenAL support: This gains us OSX and other platforms that OpenAL
  supports for sound output!
* DOS (DJGPP) support: This goes a long way to helping other DOS
  based applications like UHexen2.
* MinGW support: This gains us win32 and win64 support using this
  toolchain.
* Fedora support: We are now ready to see this get pushed upstream
  to Fedora.
* New portable file and path-name system to handle cross-platform
  support.
* Support for Debian/kFreeBSD, Debian/Hurd and other Debian archs.
* Many bug fixes, code clean-ups and cosmetic fixes.

0.3.3
* default to hidden visibility and only export our API functions
* windows lean and mean to help compile times on Windows
* cli and xcode work now on OSX
* better FreeBSD support
* Supported platforms are Debian, FreeBSD, Windows and OSX (but only
  for WAV output)

