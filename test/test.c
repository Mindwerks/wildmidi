/* tiny, minimal C source for testing, e.g. running under valgrind. */
#include <stdio.h>
#include <wildmidi_lib.h>
midi *song;
int main (int argc, char **argv) {
    if (argc != 2) return 1;
    if (WildMidi_Init("wildmidi.cfg", 44100, 0) != 0) return 1;
    song = WildMidi_Open (argv[1]);
#if defined(LIBWILDMIDI_VERSION) && (LIBWILDMIDI_VERSION-0 >= 0x000400L)
    if (!song) fprintf(stderr, "%s\n", WildMidi_GetError());
#endif
    WildMidi_MasterVolume (100);
    if (song) WildMidi_Close(song);
    WildMidi_Shutdown();
    return (song == NULL);
}
