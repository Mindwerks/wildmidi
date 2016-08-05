WildMIDI is a simple software midi player which has a core softsynth
library that can be use with other applications.

The WildMIDI library uses Gravis Ultrasound patch files to convert MIDI
files into audio which is then passed back to the calling application.
The library API is designed so that it is easy to include WildMIDI into
applications that wish to include MIDI file playback.

Version: 0.4.1
Licenses: GPLv3+ and LGPLv3
Website: http://www.mindwerks.net/projects/wildmidi

PLATFORMS:

* Linux: Arch, Debian, Fedora, Ubuntu (player: ALSA, OSS, OpenAL output)
* Windows: x86 and x64
* OSX: x86, x64 and powerpc (in Xterm. player: OpenAL output)
* BSD: Debian, FreeBSD, NetBSD, OpenBSD. (player: OSS output)
* kFreeBSD: Debian (player: OSS output)
* Hurd: Debian
* DOS (player: sound blaster or compatibles output.)

BUILD FROM SOURCE:

Requirements:
* git
* cmake
* GCC or clang / Xcode / VisualStudio / MinGW or MinGW-w64
* DOS port: DJGPP / GNU make

CHANGELOG

0.4.1
* Build: install a pkg-config file on supported platforms (Linux,...)
* File i/o updates.
* Support for AmigaOS and its variants like MorphOS and AROS.

0.4.0
* API change: The library now returns audio data in host-endian format,
  not little-endian.
* API change: WildMidi_GetVersion() added to the api, along with new
  numeric version macros in the wildmidi_lib.h header. the dso version
  is changed from 1 to 2.
* API change: All long or unsigned long type _WM_Info fields changed
  into strictly 32bit fields (int32_t or uint32_t.)
* API change: WildMidi_OpenBuffer() and WildMidi_GetOutput() changed
  to accept strictly 32bit size parameters, i.e. uint32_t, instead of
  unsigned long.
* API change: WildMidi_ConvertToMidi() and WildMidi_ConvertBufferToMidi() 
  added for MIDI-like files to be converted to MIDI.
* API change: WildMidi_SetCvtOption() added to support conversion options.
* API change: WildMidi_SongSeek() added to support Type 2 MIDI files.
* API change: WildMidi_GetLyric() added to support embedded text, 
  such as KAR files.
* API change: WildMidi_GetError() and WildMidi_ClearError() added to
  cleanly check for, retrieve and clear error messages. They no longer
  go to stderr.
* Support for loading XMI (XMIDI format) files, such as from Arena. Thanks
  Ryan Nunn for releasing his code under the LGPL.
* Support for loading MUS (MUS Id format) files, such as from Doom.
* Support for loading HMP/HMI files, such as from Daggerfall.
* Support for loading KAR (MIDI with Lyrics) and Type 2 MIDI files.
* Build requires cmake-2.8.11 or newer now.

0.3.9
* Library: Fixed a segmentation fault with bad midi files.

0.3.8
* Library: Fixed a seek-to-0 bug in order to cure an issue of truncated
  start (bug #100, gnome/gstreamer bug #694811.)
* Player, OpenAL: reduced buffers from 8 to 4 so as to cure some output
  delay issues (bug #85.)

0.3.7
* Plug a memory leak in case of broken midis.
* Properly reset global state upon library shutdown.
* Support for type-2 midi files.
* Fix a possible crash in WildMidi_SetOption.
* DOS port: Support for Sound Blaster output in player.
* Uglify the library's private global variable and function names.
* Build: Add option for a statically linked player.
* Build: Add headers to project files. Use -fno-common flag.
* Other small fixes/clean-ups.

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
  based applications.
* MinGW support: This gains us win32 and win64 support using this
  toolchain.
* OSS fixes.
* Add missing parts of the absolute paths fix in config parsing.
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

0.3.1 - 0.3.2
* Cmake updates/fixes/cleanups.

0.3.0
* initial CMake support.
* process non-registered params. fix issue of notes ending before
  attack envelope completed. (sf.net svn r149/r151.)

