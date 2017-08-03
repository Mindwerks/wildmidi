/*
 * DevTest.c: Display Information about the following file formats
 *
 *   .pat Gravis Ultrasound patch file.
 *   .mid MIDI file.
 *   .xmi Xmidi file.
 *   .hmp "HMIMIDIP" file
 *   .hmi "HMIMIDIP013195" file.
 *   .mus http://www.vgmpf.com/Wiki/index.php?title=MUS
 *
 * NOTE: This file is intended for developer use to aide in
 *       feature development, and bug hunting.
 * COMPILING: gcc -Wall -W -O2 -o devtest DevTest.c
 *
 * Copyright (C) WildMIDI Developers 2001-2016
 *
 * This file is part of WildMIDI.
 *
 * WildMIDI is free software: you can redistribute and/or modify the library
 * under the terms of the GNU Lesser General Public License and you can
 * redistribute the player and DevTest under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the licenses, or(at your option) any later version.
 *
 * WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 * the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License and the
 * GNU Lesser General Public License along with WildMIDI.  If not,  see
 * <http://www.gnu.org/licenses/>.
 */

/*#include "config.h"*/

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <getopt_long.h>
#include <io.h>
#undef close
#define close _close
#undef open
#define open _open
#undef read
#define read _read
#else /* unixish: */
#include <unistd.h>
#include <pwd.h>
#include <getopt.h>
#endif

#define UNUSED(x) (void)(x)

static struct option const long_options[] = {
    { "debug-level", 1, 0, 'd' },
    { "version", 0, 0, 'v' },
    { "help", 0, 0, 'h' },
    { NULL, 0, NULL, 0 }
};

#define EVENT_DATA_8BIT 1


static float env_time_table[] = {
    0.0f,         0.091728000f, 0.045864000f, 0.030576000f, 0.022932000f, 0.018345600f, 0.015288000f, 0.013104000f,
    0.011466000f, 0.010192000f, 0.009172800f, 0.008338909f, 0.007644000f, 0.007056000f, 0.006552000f, 0.006115200f,
    0.005733000f, 0.005395765f, 0.005096000f, 0.004827789f, 0.004586400f, 0.004368000f, 0.004169455f, 0.003988174f,
    0.003822000f, 0.003669120f, 0.003528000f, 0.003397333f, 0.003276000f, 0.003163034f, 0.003057600f, 0.002958968f,
    0.002866500f, 0.002779636f, 0.002697882f, 0.002620800f, 0.002548000f, 0.002479135f, 0.002413895f, 0.002352000f,
    0.002293200f, 0.002237268f, 0.002184000f, 0.002133209f, 0.002084727f, 0.002038400f, 0.001994087f, 0.001951660f,
    0.001911000f, 0.001872000f, 0.001834560f, 0.001798588f, 0.001764000f, 0.001730717f, 0.001698667f, 0.001667782f,
    0.001638000f, 0.001609263f, 0.001581517f, 0.001554712f, 0.001528800f, 0.001503738f, 0.001479484f, 0.001456000f,

    0.0f,         0.733824000f, 0.366912000f, 0.244608000f, 0.183456000f, 0.146764800f, 0.122304000f, 0.104832000f,
    0.091728000f, 0.081536000f, 0.073382400f, 0.066711273f, 0.061152000f, 0.056448000f, 0.052416000f, 0.048921600f,
    0.045864000f, 0.043166118f, 0.040768000f, 0.038622316f, 0.036691200f, 0.034944000f, 0.033355636f, 0.031905391f,
    0.030576000f, 0.029352960f, 0.028224000f, 0.027178667f, 0.026208000f, 0.025304276f, 0.024460800f, 0.023671742f,
    0.022932000f, 0.022237091f, 0.021583059f, 0.020966400f, 0.020384000f, 0.019833081f, 0.019311158f, 0.018816000f,
    0.018345600f, 0.017898146f, 0.017472000f, 0.017065674f, 0.016677818f, 0.016307200f, 0.015952696f, 0.015613277f,
    0.015288000f, 0.014976000f, 0.014676480f, 0.014388706f, 0.014112000f, 0.013845736f, 0.013589333f, 0.013342255f,
    0.013104000f, 0.012874105f, 0.012652138f, 0.012437695f, 0.012230400f, 0.012029902f, 0.011835871f, 0.011648000f,

    0.0f,         5.870592000f, 2.935296000f, 1.956864000f, 1.467648000f, 1.174118400f, 0.978432000f, 0.838656000f,
    0.733824000f, 0.652288000f, 0.587059200f, 0.533690182f, 0.489216000f, 0.451584000f, 0.419328000f, 0.391372800f,
    0.366912000f, 0.345328941f, 0.326144000f, 0.308978526f, 0.293529600f, 0.279552000f, 0.266845091f, 0.255243130f,
    0.244608000f, 0.234823680f, 0.225792000f, 0.217429333f, 0.209664000f, 0.202434207f, 0.195686400f, 0.189373935f,
    0.183456000f, 0.177896727f, 0.172664471f, 0.167731200f, 0.163072000f, 0.158664649f, 0.154489263f, 0.150528000f,
    0.146764800f, 0.143185171f, 0.139776000f, 0.136525395f, 0.133422545f, 0.130457600f, 0.127621565f, 0.124906213f,
    0.122304000f, 0.119808000f, 0.117411840f, 0.115109647f, 0.112896000f, 0.110765887f, 0.108714667f, 0.106738036f,
    0.104832000f, 0.102992842f, 0.101217103f, 0.099501559f, 0.097843200f, 0.096239213f, 0.094686968f, 0.093184000f,

    0.0f,        46.964736000f,23.482368000f,15.654912000f,11.741184000f, 9.392947200f, 7.827456000f, 6.709248000f,
    5.870592000f, 5.218304000f, 4.696473600f, 4.269521455f, 3.913728000f, 3.612672000f, 3.354624000f, 3.130982400f,
    2.935296000f, 2.762631529f, 2.609152000f, 2.471828211f, 2.348236800f, 2.236416000f, 2.134760727f, 2.041945043f,
    1.956864000f, 1.878589440f, 1.806336000f, 1.739434667f, 1.677312000f, 1.619473655f, 1.565491200f, 1.514991484f,
    1.467648000f, 1.423173818f, 1.381315765f, 1.341849600f, 1.304576000f, 1.269317189f, 1.235914105f, 1.204224000f,
    1.174118400f, 1.145481366f, 1.118208000f, 1.092203163f, 1.067380364f, 1.043660800f, 1.020972522f, 0.999249702f,
    0.978432000f, 0.958464000f, 0.939294720f, 0.920877176f, 0.903168000f, 0.886127094f, 0.869717333f, 0.853904291f,
    0.838656000f, 0.823942737f, 0.809736828f, 0.796012475f, 0.782745600f, 0.769913705f, 0.757495742f, 0.745472000f
};

/* the following hardcoded to avoid the need for a config.h : */
static const char *PACKAGE_URL = "http://www.mindwerks.net/projects/wildmidi/";
static const char *PACKAGE_BUGREPORT = "https://github.com/Mindwerks/wildmidi/issues";
static const char *PACKAGE_VERSION = "0.4";

static void do_version(void) {
    printf("DevTest for WildMIDI %s - For testing purposes only\n", PACKAGE_VERSION);
    printf("Copyright (C) WildMIDI Developers 2001-2014\n");
    printf("DevTest comes with ABSOLUTELY NO WARRANTY\n");
    printf("This is free software, and you are welcome to redistribute it\n");
    printf("under the terms and conditions of the GNU Lesser General Public License version 3.\n");
    printf("For more information see COPYING\n\n");
    printf("Report bugs to %s\n", PACKAGE_BUGREPORT);
    printf("WildMIDI homepage at %s\n", PACKAGE_URL);
    printf("\n");
}

static void do_help(void) {
    do_version();
    printf(" -d N   --debug-level N    Verbose output\n");
    printf(" -h     --help             Display this information\n");
    printf(" -v     --version          Display version information\n\n");
}

static unsigned char *
DT_BufferFile(const char *filename, unsigned long int *size) {
    int buffer_fd;
    unsigned char *data;
    char *ret_data = NULL;
    struct stat buffer_stat;
#ifndef _WIN32
    const char *home = NULL;
    struct passwd *pwd_ent;
    char buffer_dir[1024];
#endif
    char *buffer_file = NULL;

#ifndef _WIN32
    if (strncmp(filename, "~/", 2) == 0) {
        if ((pwd_ent = getpwuid(getuid()))) {
            home = pwd_ent->pw_dir;
        } else {
            home = getenv("HOME");
        }
        if (home) {
            buffer_file = malloc(strlen(filename) + strlen(home) + 1);
            if (buffer_file == NULL) {
                printf("Unable to get ram to expand %s: %s\n", filename,
                        strerror(errno));
                return NULL;
            }
            strcpy(buffer_file, home);
            strcat(buffer_file, filename + 1);
        }
    } else if (filename[0] != '/') {
        ret_data = getcwd(buffer_dir, 1024);
        if (ret_data != NULL)
            buffer_file = malloc(strlen(filename) + strlen(buffer_dir) + 2);
        if (buffer_file == NULL || ret_data == NULL) {
            printf("Unable to get ram to expand %s: %s\n", filename,
                    strerror(errno));
            return NULL;
        }
        strcpy(buffer_file, buffer_dir);
        if (buffer_dir[strlen(buffer_dir) - 1] != '/')
            strcat(buffer_file, "/");
        strcat(buffer_file, filename);
    }
#endif

    if (buffer_file == NULL) {
        buffer_file = malloc(strlen(filename) + 1);
        if (buffer_file == NULL) {
            printf("Unable to get ram to expand %s: %s\n", filename,
                    strerror(errno));
            return NULL;
        }
        strcpy(buffer_file, filename);
    }

    if (stat(buffer_file, &buffer_stat)) {
        printf("Unable to stat %s: %s\n", filename, strerror(errno));
        free(buffer_file);
        return NULL;
    }
    *size = buffer_stat.st_size;
    data = malloc(*size);
    if (data == NULL) {
        printf("Unable to get ram for %s: %s\n", filename, strerror(errno));
        free(buffer_file);
        return NULL;
    }
#ifdef _WIN32
    if ((buffer_fd = open(buffer_file,(O_RDONLY | O_BINARY))) == -1) {
#else
    if ((buffer_fd = open(buffer_file, O_RDONLY)) == -1) {
#endif
        printf("Unable to open %s: %s\n", filename, strerror(errno));
        free(buffer_file);
        free(data);
        return NULL;
    }
    if (read(buffer_fd, data, *size) != buffer_stat.st_size) {
        printf("Unable to read %s: %s\n", filename, strerror(errno));
        free(buffer_file);
        free(data);
        close(buffer_fd);
        return NULL;
    }
    close(buffer_fd);
    free(buffer_file);
    return data;
}

static char check_notes[16][128];

static void zero_check_notes(void) {

}

static int count_check_notes(void) {
    int ch = 0;
    int nt = 0;
    int notes_still_on = 0;

    for (ch = 0; ch < 16; ch++) {
        for (nt = 0; nt < 128; nt++) {
            if (check_notes[ch][nt] == 1) {
                notes_still_on++;
            }
        }
    }

    return notes_still_on;
}

static int check_midi_event (unsigned char *midi_data, unsigned long int midi_size,
                             unsigned char running_event, int verbose, int options) {
    unsigned int rtn_cnt = 0;
    unsigned char event = 0;
    unsigned int meta_length = 0;
    unsigned int i = 0;
    /*
    printf("Midi Data: ");
    for (i = 0; i < ((midi_size >= 32)? 32 : midi_size); i++) {
        printf("0x%.2x ",midi_data[i]);
        if (i == 15) printf("\n           ");
    }
    printf("\n");

    printf("Midi Size: %lu\n", midi_size);
    */
    if (*midi_data < 0x80) {
        if (running_event != 0) {
            event = running_event;
        } else {
            printf("Expected MIDI event\n");
            return -1;
        }
//      printf("Unsing running event 0x%2x\n", running_event);
    } else {
        event = *midi_data++;
        midi_size--;
        rtn_cnt++;
    }

    switch (event >> 4) {
        case 0x8:
            if ((midi_size < 2) || (midi_data[0] > 0x7F)
                || (midi_data[1] > 0x7F)) {
                printf("Note off: Missing or Corrupt MIDI Data\n");
                return -1;
            }
            if (verbose)
                printf("Note Off: chan(%i) note(%i) vel(%i)\n",
                       (event & 0x0F), midi_data[0], midi_data[1]);
            rtn_cnt += 2;
            check_notes[(event & 0x0F)][midi_data[0]] = 0;
            break;
        case 0x9:
            if ((midi_size < 2) || (midi_data[0] > 0x7F)
                || (midi_data[1] > 0x7F)) {
                printf("Note On: Missing or Corrupt MIDI Data\n");
                return -1;
            }
            if (verbose)
                printf("Note On: chan(%i) note(%i) vel(%i)\n",
                       (event & 0x0F), midi_data[0], midi_data[1]);
            rtn_cnt += 2;
            if (midi_data[1] == 0) {
                check_notes[(event & 0x0F)][midi_data[0]] = 0;
            } else {
                check_notes[(event & 0x0F)][midi_data[0]] = 1;
            }
            break;
        case 0xA:
            if ((midi_size < 2) || (midi_data[0] > 0x7F)
                || (midi_data[1] > 0x7F)) {
                printf("Aftertouch: Missing or Corrupt MIDI Data\n");
                return -1;
            }
            if (verbose)
                printf("Aftertouch: chan(%i) note(%i) vel(%i)\n",
                       (event & 0x0F), midi_data[0], midi_data[1]);
            rtn_cnt += 2;
            break;
        case 0xB:
            if (!(options & EVENT_DATA_8BIT)) {
                if ((midi_size < 2) || (midi_data[0] > 0x7F)
                        || (midi_data[1] > 0x7F)) {
                    printf("Controller: Missing or Corrupt MIDI Data\n");
                    return -1;
                }
            } else {
                if ((midi_size < 2) || (midi_data[0] > 0x7F)) {
                    printf("Controller: Missing or Corrupt MIDI Data\n");
                    return -1;
                }

            }
            if (verbose) {
                printf("Controller ");
                switch(midi_data[0]) {
                    case 0:
                        printf("Bank Select: ");
                        break;
                    case 6:
                        printf("Data Entry Course: ");
                        break;
                    case 7:
                        printf("Channel Volume: ");
                        break;
                    case 8:
                        printf("Channel Balance: ");
                        break;
                    case 10:
                        printf("Pan: ");
                        break;
                    case 11:
                        printf("Channel Expression: ");
                        break;
                    case 38:
                        printf("Data Entry Fine: ");
                        break;
                    case 64:
                        printf("Channel Hold: ");
                        break;
                    case 96:
                        printf("Data Increment: ");
                        break;
                    case 97:
                        printf("Data Decrement: ");
                        break;
                    case 98:
                        printf("Non-registered Param Fine: ");
                        break;
                    case 99:
                        printf("Non-registered Param Course: ");
                        break;
                    case 100:
                        printf("Registered Param Fine: ");
                        break;
                    case 101:
                        printf("Registered Param Course: ");
                        break;
                    case 120:
                        printf("Channel Sound Off: ");
                        break;
                    case 121:
                        printf("Channel Controllers Off: ");
                        break;
                    case 123:
                        printf("Channel Notes Off: ");
                        break;
                    default: printf(" Unknown(%i): ", midi_data[0]);
                }
                printf("chan(%i) set(%i)\n", (event & 0x0F), midi_data[1]);
            }
            rtn_cnt += 2;
            break;
        case 0xC:
            if ((midi_size == 0) || (*midi_data > 0x7F)) {
                printf("Set Patch: Missing or Corrupt MIDI Data\n");
                return -1;
            }
            if (verbose)
                printf("Set Patch: chan(%i) patch(%i)\n", (event & 0x0F),
                       *midi_data);
            rtn_cnt++;
            break;
        case 0xD:
            if ((midi_size == 0) || (*midi_data > 0x7F)) {
                printf("Channel Pressure: Missing or Corrupt MIDI Data\n");
                return -1;
            }
            if (verbose)
                printf("Channel Pressure: chan(%i) pres(%i)\n",
                       (event & 0x0F), *midi_data);
            rtn_cnt++;
            break;
        case 0xE:
            if ((midi_size < 2) || (midi_data[0] > 0x7F)
                || (midi_data[1] > 0x7F)) {
                printf("Set Pitch: Missing or Corrupt MIDI Data\n");
                return -1;
            }
            if (verbose)
                printf("Set Pitch: chan(%i) pitch(%i)\n", (event & 0x0F),
                       ((midi_data[0] << 7) | midi_data[1]));
            rtn_cnt += 2;
            break;
        case 0xF:
            if ((event == 0xF0) || (event == 0xF7)) {
                unsigned long int sysex_size = 0;
                unsigned char *sysex_store = NULL;
                unsigned long int sysex_store_ofs = 0;

                if (verbose)
                    printf("Sysex Event which we mostly ignore\n");

                while (*midi_data > 0x7F) {
                    sysex_size = (sysex_size << 7) | (*midi_data & 0x7F);
                    midi_data++;
                    midi_size--;
                    rtn_cnt++;
                }
                sysex_size = (sysex_size << 7) | (*midi_data & 0x7F);
                midi_data++;
                midi_size--;
                rtn_cnt++;
                if (!sysex_size) {
                    if (verbose) printf("Zero Sysex size\n");
                    break;
                }
                if (sysex_size > midi_size) {
                    printf("Bad Sysex size %lu\n", sysex_size);
                    return -1;
                }

                sysex_store = realloc(sysex_store,
                                      (sysex_store_ofs + sysex_size));
                memcpy(&sysex_store[sysex_store_ofs], midi_data,
                       sysex_size);
                sysex_store_ofs += sysex_size;

                if (sysex_store[sysex_store_ofs - 1] == 0xF7) {
                    unsigned long int sysex_ofs = 0;
                    unsigned char tmpsysexdata[] = { 0x41, 0x10, 0x42, 0x12 };
                    if (strncmp((const char *) tmpsysexdata,
                                (const char *) sysex_store, 4) == 0) {
                        /* Roland Sysex Checksum */
                        unsigned char sysex_cs = 0;
                        sysex_ofs = 4;

                        do {
                            sysex_cs += sysex_store[sysex_ofs];
                            if (sysex_cs > 0x7F) {
                                sysex_cs -= 0x80;
                            }
                            sysex_ofs++;
                        } while (sysex_store[sysex_ofs + 1] != 0xF7);
                        sysex_cs = 0x80 - sysex_cs;
                        if (sysex_cs != sysex_store[sysex_ofs]) {
                            printf("Roland Sysex Checksum Error: ");
                            sysex_ofs = 0;
                            do {
                                printf("%02x ", sysex_store[sysex_ofs]);
                                sysex_ofs++;
                            } while (sysex_ofs != sysex_store_ofs);
                            printf("\n");
                            free(sysex_store);
                            return -1;
                        } else {
                            if (sysex_store[4] == 0x40) {
                                if (((sysex_store[5] & 0xF0) == 0x10)
                                    && (sysex_store[6] == 0x15)) {
                                    /* Roland Drum Track Setting */
                                    unsigned char sysex_ch = 0x0F & sysex_store[5];
                                    if (sysex_ch == 0x00) {
                                        sysex_ch = 0x09;
                                    } else if (sysex_ch <= 0x09) {
                                        sysex_ch -= 1;
                                    }
                                    if (verbose)
                                        printf("Additional Drum Channel(0x%02x) Setting: 0x%02x\n",
                                               sysex_ch, sysex_store[7]);
                                } else if ((sysex_store[5] == 0x00)
                                           && (sysex_store[6] == 0x7F)
                                           && (sysex_store[7] == 0x00)) {
                                    /* Roland GS Reset */
                                    if (verbose)
                                        printf("GS Reset\n");
                                } else {
                                    goto UNKNOWNSYSEX;
                                }
                            } else {
                                goto UNKNOWNSYSEX;
                            }
                        }
                    } else {
                    UNKNOWNSYSEX: if (verbose) {
                        printf("Unknown Sysex: ");
                        sysex_ofs = 0;
                        do {
                            printf("%02x ", sysex_store[sysex_ofs]);
                            sysex_ofs++;
                        } while (sysex_ofs != sysex_store_ofs);
                        printf("\n");
                    }
                    }
                }
                free(sysex_store);
                sysex_store = NULL;
                rtn_cnt += sysex_size;
            } else if ((event <= 0xFE) && (event >= 0xF1)) {
                // Added just in case
                printf("Realtime Event: 0x%.2x ** NOTE: Not expected in midi file type data\n",event);
            } else if (event == 0xFF) {
                /*
                 * Only including meta events that are supported by wildmidi
                 */
                if (*midi_data == 0x00) {
                    if (midi_size < 4) {
                        printf("Data too short: Missing MIDI Data\n");
                        return -1;
                    }
                    if (midi_data[1] != 0x02) {
                        printf("Corrupt MIDI Data, Bad Sequence Number Prefix\n");
                        return -1;
                    }
                    if (verbose) {
                        printf("Meta Event: Sequence Number: %.2x %.2x\n", midi_data[2], midi_data[3]);
                    }

                } else if (*midi_data == 0x01) {
                    if (verbose)
                        printf("Meta Event: Text\n");
                } else if (*midi_data == 0x02) {
                    if (verbose)
                        printf("Meta Event: Copyright\n");
                } else if (*midi_data == 0x03) {
                    if (verbose)
                        printf("Meta Event: Trackname\n");
                } else if (*midi_data == 0x04) {
                    if (verbose)
                        printf("Meta Event: Instrument Name\n");
                } else if (*midi_data == 0x05) {
                    if (verbose)
                        printf("Meta Event: Lyric\n");
                } else if (*midi_data == 0x06) {
                    if (verbose)
                        printf("Meta Event: Marker\n");
                } else if (*midi_data == 0x07) {
                    if (verbose)
                        printf("Meta Event: Cue Point\n");
                } else if (*midi_data == 0x2F) {
                    if (verbose) {
                        printf("Meta Event: End Of Track\n");
                        printf("========================\n\n");
                    }
                    if (midi_size < 2) {
                        printf("Data too short: Missing MIDI Data\n");
                        return -1;
                    }
                    if (midi_data[1] != 0x00) {
                        printf("Missing or Corrupt MIDI Data\n");
                        return -1;
                    }
                } else if (*midi_data == 0x20) {
                    if (verbose)
                        printf("Meta Event: Channel Prefix: ");
                    if (midi_size < 3) {
                        printf("Data too short: Missing MIDI Data\n");
                        return -1;
                    }
                    if (midi_data[1] != 0x01) {
                        printf("Corrupt MIDI Data, Bad Channel Prefix\n");
                        return -1;
                    }
                    if (verbose)
                        printf("%i\n", (int) midi_data[2]);
                } else if (*midi_data == 0x21) {
                    if (verbose)
                        printf("Meta Event: Port Prefix: ");
                    if (midi_size < 3) {
                        printf("Data too short: Missing MIDI Data\n");
                        return -1;
                    }
                    if (midi_data[1] != 0x01) {
                        printf("Corrupt MIDI Data, Bad Port Prefix\n");
                        return -1;
                    }
                    if (verbose)
                        printf("%i\n", (int) midi_data[2]);
                } else if (*midi_data == 0x51) {
                    if (verbose)
                        printf("Meta Event: Tempo\n");
                    if (midi_size < 5) {
                        printf("Data too short: Missing MIDI Data\n");
                        return -1;
                    }
                    if (midi_data[1] != 0x03) {
                        printf("Corrupt MIDI Data, Bad Tempo\n");
                        return -1;
                    }
                } else if (*midi_data == 0x54) {
                    if (midi_size < 7) {
                        printf("Data too short: Missing MIDI Data\n");
                        return -1;
                    }
                    if (midi_data[1] != 0x05) {
                        printf("Corrupt MIDI Data, Bad SMPTE Offset Prefix\n");
                        return -1;
                    }
                    if (verbose) {
                        printf("Meta Event: SMPTE Offset: %.2x %.2x %.2x %.2x %2.x\n", midi_data[2], midi_data[3], midi_data[4], midi_data[5], midi_data[6]);
                    }

                } else if (*midi_data == 0x58) {
                    if (midi_size < 6) {
                        printf("Data too short: Missing MIDI Data\n");
                        return -1;
                    }
                    if (midi_data[1] != 0x04) {
                        printf("Corrupt MIDI Data, Bad Time Signature Prefix\n");
                        return -1;
                    }
                    if (verbose) {
                        printf("Meta Event: Time Signature: %.2x %.2x %.2x %.2x\n", midi_data[2], midi_data[3], midi_data[4], midi_data[5]);
                    }

                } else if (*midi_data == 0x59) {
                    if (midi_size < 4) {
                        printf("Data too short: Missing MIDI Data\n");
                        return -1;
                    }
                    if (midi_data[1] != 0x02) {
                        printf("Corrupt MIDI Data, Bad Key Signature Prefix\n");
                        return -1;
                    }
                    if (verbose) {
                        printf("Meta Event: Key Signature: %.2x %.2x\n", midi_data[2], midi_data[3]);
                    }

                } else {
                    if (verbose)
                        printf("Meta Event: Unsupported (%i)\n",
                               *midi_data);
                }
                midi_data++;
                midi_size--;
                rtn_cnt++;
                meta_length = 0;
                while (*midi_data > 0x7F) {
                    meta_length = (meta_length << 7) | (*midi_data & 0x7F);
                    midi_data++;
                    rtn_cnt++;
                    if (midi_size == 0) {
                        printf("Data too short: Missing MIDI Data\n");
                        return -1;
                    }
                    midi_size--;
                }
                meta_length = (meta_length << 7) | (*midi_data & 0x7F);
                midi_data++;
                midi_size--;
                rtn_cnt++;

                if (midi_size < meta_length) {
                    printf("Data too short: Missing MIDI Data\n");
                    return -1;
                }

                if ((verbose) && (meta_length != 0)) {
                    printf ("Meta data (%u bytes):", meta_length);
                    for (i = 0; i < meta_length; i++) {
                        if ((i % 4) == 0) printf(" ");
                        if ((i % 8) == 0) printf("\n");
                        printf ("0x%.2x ", *midi_data);
                        if (*midi_data > 32) printf("%c ", *midi_data);
                        midi_data++;
                    }
                    printf("\n");
                }
                rtn_cnt += meta_length;
            } else {
                printf("Corrupt Midi, Unknown Event Data\n");
                return -1;
            }
            break;
    }
//  printf("Return Count: %i\n", rtn_cnt);
    return rtn_cnt;
}

static uint32_t time_mins = 0;
static float time_secs = 0.0f;
static float secs_per_tick = 0.0f;
static float frequency = 0.0f;

static void set_secs_per_tick (uint32_t divisions, uint32_t tempo) {
    float microseconds_per_tick;

    /* Slow but needed for accuracy */
    microseconds_per_tick = (float) tempo / (float) divisions;
    secs_per_tick = microseconds_per_tick / 1000000.0f;

    return;
}

static void add_time (int add_ticks) {
    uint32_t add_mins = 0;

    time_secs += (add_ticks * secs_per_tick);
    if (time_secs > 60.0f) {
        add_mins = (uint32_t)(time_secs / 60.0f);
        time_secs -= (float)(add_mins * 60);
        time_mins += add_mins;
    }
    return;
}

static void display_time(void) {
    printf("Time %.2i:%02.05f    ", time_mins, time_secs);
    return;
}

static void add_and_display_time(int add_ticks) {
    add_time(add_ticks);
    display_time();
    return;
}


static int8_t test_mus(uint8_t * mus_data, uint32_t mus_size, uint32_t verbose) {
    uint32_t mus_data_ofs = 0;
    uint32_t mus_song_ofs = 0;
    uint32_t mus_song_len = 0;
//  uint16_t mus_ch_cnt1 = 0;
//  uint16_t mus_ch_cnt2 = 0;
    uint16_t mus_no_instr = 0;
    uint16_t mus_instr_cnt = 0;
    uint8_t mus_event_size = 0;
    uint8_t mus_hdr[] = { 'M', 'U', 'S', 0x1A };
    uint8_t mus_prev_vol[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint32_t mus_ticks = 0;

    // Check that we have enough data to check the header
    if (mus_size < 17) {
        printf("Not a valid MUS file: File too short\n");
        return -1;
    }

    // Check Header
    if (strncmp((char *) mus_data, (char *) mus_hdr, 4) != 0) {
        printf("Not a valid MUS file: expected MUS followed by 0x1A\n");
        return -1;
    }

    // Get Song Length
    mus_song_len = (mus_data[5] << 8) | mus_data[4];
    // Get Song Offset
    mus_song_ofs = (mus_data[7] << 8) | mus_data[6];

    if (verbose) printf("Song Offset: %i, Length: %i\n", mus_song_ofs, mus_song_len);

    // Have yet to determine what this actually is.
    // mus_ch_cnt1 = (mus_data[9] << 8) | mus_data[8];
    // mus_ch_cnt2 = (mus_data[11] << 8) | mus_data[10];

    // Number of instruments defined
    mus_no_instr = (mus_data[13] << 8) | mus_data[12];
    if (verbose) printf("Number of Instruments: %i\n", mus_no_instr);

    // Skip next 2 data bytes
    mus_data_ofs = 16;

    // Check that we have enough data to check the rest
    if (mus_size < (mus_data_ofs + (mus_no_instr << 1) + mus_song_len)) {
        printf("Not a valid MUS file: File too short\n");
        return -1;
    }

    if (verbose) {
        do {
            printf("(%i) %i\n",mus_instr_cnt, ((mus_data[mus_data_ofs + 1] << 8) | mus_data[mus_data_ofs]));
            mus_data_ofs += 2;
            mus_instr_cnt++;
        } while (mus_instr_cnt != mus_no_instr);
    }

    // make sure we are at song offset
    mus_data_ofs = mus_song_ofs;

    if (verbose) {
        // Setup secs_per_tick
        if (frequency == 0.0) frequency = 140.0;
        set_secs_per_tick (60, (uint32_t)(60000000.0f / frequency));
        add_and_display_time(0);
    }

    do {
        // Read Event
    _WM_READ_MUS_EVENT:
        if (verbose) {
            display_time();
        }
        switch ((mus_data[mus_data_ofs] >> 4) & 0x07) {
            case 0: // note off
                mus_event_size = 2;
                if (verbose) printf("Note Off 0x%.2x\n", mus_data[mus_data_ofs + 1]);
                break;
            case 1: // note on
                if (mus_data[mus_data_ofs + 1] & 0x80) {
                    mus_event_size = 3;
                    if (verbose) printf("Note On (0x%.2x): 0x%.2x\n", (mus_data[mus_data_ofs + 1] & 0x7f), mus_data[mus_data_ofs + 2]);
                    mus_prev_vol[mus_data[mus_data_ofs] & 0x0f] = mus_data[mus_data_ofs + 2];
                } else {
                    mus_event_size = 2;
                    if (verbose) printf("Note On (0x%.2x): [0x%.2x]\n", mus_data[mus_data_ofs + 1], mus_prev_vol[mus_data[mus_data_ofs] & 0x0f]);
                }
                break;
            case 2: // pitch bend
                mus_event_size = 2;
                if (verbose) printf("Pitch Bend 0x%.2x\n", mus_data[mus_data_ofs + 1]);
                break;
            case 3: // system controller
                mus_event_size = 2;
                if (verbose) {
                    printf("System Controller: ");
                    switch (mus_data[mus_data_ofs + 1]) {
                        case 10:
                            printf("All Sounds Off\n");
                            break;
                        case 11:
                            printf("All Notes Off\n");
                            break;
                        case 12: // Not supported by WildMidi. Parsed for compatability
                            printf("Mono\n");
                            break;
                        case 13:  // Not supported by WildMidi. Parsed for compatability
                            printf("Poly\n");
                            break;
                        case 14:
                            printf("Reset All Controllers\n");
                            break;
                        default:
                            printf("Unsupported %i\n", mus_data[mus_data_ofs + 1]);
                    }
                }
                break;
            case 4: // controller
                mus_event_size = 3;
                if (verbose) {
                    printf("Controller: ");
                    switch (mus_data[mus_data_ofs + 1]) {
                        case 0:
                            printf("Patch 0x%.2x\n", mus_data[mus_data_ofs + 2]);
                            break;
                        case 1:
                            printf("Bank 0x%.2x\n", mus_data[mus_data_ofs + 2]);
                            break;
                        case 2:
                            printf("Modulation 0x%.2x\n", mus_data[mus_data_ofs + 2]);
                            break;
                        case 3:
                            printf("Volume 0x%.2x\n", mus_data[mus_data_ofs + 2]);
                            break;
                        case 4:
                            printf("Pan 0x%.2x\n", (mus_data[mus_data_ofs + 2] - 64));
                            break;
                        case 5:
                            printf("Expression 0x%.2x\n", mus_data[mus_data_ofs + 2]);
                            break;
                        case 6:
                            printf("Reverb 0x%.2x\n", mus_data[mus_data_ofs + 2]);
                            break;
                        case 7:
                            printf("Chorus 0x%.2x\n", mus_data[mus_data_ofs + 2]);
                            break;
                        case 8:
                            printf("Sustain 0x%.2x\n", mus_data[mus_data_ofs + 2]);
                            break;
                        case 9:
                            printf("Soft Pedal 0x%.2x\n", mus_data[mus_data_ofs + 2]);
                            break;
                        default:
                            printf("Unsupported 0x%.2x 0x%.2x\n", mus_data[mus_data_ofs + 1], mus_data[mus_data_ofs + 2]);
                    }
                }
                break;
            case 5: // ??
                mus_event_size = 1;
                if (verbose) printf("0x%.2x\n", mus_data[mus_data_ofs]);
                break;
            case 6: // End Of Song
                //mus_event_size = 1;
                if (verbose) printf("End Of Song\n");
                goto _WM_MUS_EOS;
                break;
            case 7: // ??
                mus_event_size = 1;
                if (verbose) printf("0x%.2x\n", mus_data[mus_data_ofs]);
                break;
        }
#if 0
        // DEBUG
        if (verbose)
        {
            uint8_t i = 0;
            for (i = 0; i < mus_event_size; i++) {
                printf("0x%.2x ", mus_data[mus_data_ofs + i]);
            }
            printf("\n");
        }
#endif
        if (!(mus_data[mus_data_ofs] & 0x80)) {
            mus_data_ofs += mus_event_size;
            goto _WM_READ_MUS_EVENT;
        }
        mus_data_ofs += mus_event_size;

        // Read Time (140 ticks per minute)
        mus_ticks = 0;
        do {
            mus_ticks = (mus_ticks << 7) | (mus_data[mus_data_ofs] & 0x7f);
            mus_data_ofs++;
        } while (mus_data[mus_data_ofs - 1] > 0x7f);
        if (verbose) {
            add_time(mus_ticks);
        }
    } while (mus_data_ofs < mus_size);

    // Song End
_WM_MUS_EOS:
    return 0;
}

static int test_hmi(unsigned char * hmi_data, unsigned long int hmi_size, int verbose) {
    uint16_t hmi_division = 0;
    //uint32_t hmi_duration_secs = 0;
    uint32_t hmi_track_cnt = 0;
    uint32_t i = 0;
    uint32_t *hmi_track_offset = NULL;
    uint32_t hmi_dbg = 0;
    uint32_t hmi_delta = 0;
    uint32_t hmi_track_end = 0;
    int32_t check_ret = 0;
    uint8_t hmi_running_event = 0;
    uint32_t hmi_track_header_length = 0;
    uint32_t hmi_file_end = hmi_size;

    // Check header
    if (strncmp((char *) hmi_data,"HMI-MIDISONG061595", 18) != 0) {
        printf("Not a valid HMI file: expected HMI-MIDISONG061595\n");
        return -1;
    }

    hmi_data += 212;
    hmi_size -= 212;
    hmi_dbg += 212;

    hmi_division = *hmi_data++;
    if (verbose) {
        printf("Beats per minute: %u\n",hmi_division);
        set_secs_per_tick (60, (uint32_t)(60000000.0f / (float)hmi_division));
    }

    hmi_data += 15;
    hmi_size -= 16;
    hmi_dbg += 16;

    hmi_track_cnt = *hmi_data++;
    hmi_size--;
    if (verbose) printf("Track count: %i\n", hmi_track_cnt);
    hmi_track_offset = malloc(sizeof(uint32_t) * hmi_track_cnt);
    hmi_dbg++;

    hmi_data += 141;
    hmi_size -= 141;
    hmi_dbg += 141;

    hmi_track_offset[0] = *hmi_data; // To keep Xcode happy

    for (i = 0; i < hmi_track_cnt; i++) {
//      printf("DEBUG @ %.8x\n",hmi_dbg);
        hmi_track_offset[i] = *hmi_data++;
        hmi_track_offset[i] += (*hmi_data++ << 8);
        hmi_track_offset[i] += (*hmi_data++ << 16);
        hmi_track_offset[i] += (*hmi_data++ << 24);
        hmi_size -= 4;
        //FIXME: These are absolute data offsets?
        if (verbose) printf("Track %i offset: %.8x\n",i,hmi_track_offset[i]);
        hmi_dbg += 4;
    }

    hmi_size -= (hmi_track_offset[0] - hmi_dbg);
    hmi_data += (hmi_track_offset[0] - hmi_dbg);
    hmi_dbg += (hmi_track_offset[0] - hmi_dbg);
    for (i = 0; i < hmi_track_cnt; i++) {
        /*
        printf("DEBUG @ %.8x: ",hmi_dbg);
        for (j = 0; j < 16; j++) {
            printf("%.2x ",hmi_data[j]);
        }
        printf("\n");
        */
        if (strncmp((char *) hmi_data,"HMI-MIDITRACK", 13) != 0) {
            printf("Not a valid HMI file: expected HMI-MIDITRACK\n");
            free (hmi_track_offset);
            return -1;
        }
        if (verbose) printf("Start of track %u\n",i);

        hmi_track_header_length = hmi_data[0x57];
        hmi_track_header_length += (hmi_data[0x58] << 8);
        hmi_track_header_length += (hmi_data[0x59] << 16);
        hmi_track_header_length += (hmi_data[0x5a] << 24);
        if (verbose) printf("Track header length: %u\n",hmi_track_header_length);

        hmi_data += hmi_track_header_length;
        hmi_size -= hmi_track_header_length;
        hmi_dbg += hmi_track_header_length;

        if (i < (hmi_track_cnt -1)) {
            hmi_track_end = hmi_track_offset[i+1];
        } else {
            hmi_track_end = hmi_file_end;
        }
//      printf("DEBUG: 0x%.8x\n",hmi_track_end);

        while (hmi_dbg < hmi_track_end) {
            /*
            printf("DEBUG @ 0x%.8x: ",hmi_dbg);
            for (j = 0; j < 16; j++) {
                printf("%.2x ",hmi_data[j]);
            }
            printf("\n");
            */
            hmi_delta = 0;
            if (*hmi_data > 0x7f) {
                while (*hmi_data > 0x7F) {
                    hmi_delta = (hmi_delta << 7) | (*hmi_data & 0x7F);
                    hmi_data++;
                    hmi_size--;
                    hmi_dbg++;
                }
            }
            hmi_delta = (hmi_delta << 7) | (*hmi_data & 0x7F);
            if (verbose) {
                add_and_display_time(hmi_delta);
            }
            hmi_data++;
            hmi_size--;
            hmi_dbg++;

            if (hmi_data[0] == 0xfe) {
                if (verbose) printf("Skipping HMI event\n");
                if (hmi_data[1] == 0x10) {
                    hmi_size -= (hmi_data[4] + 5);
                    hmi_dbg += (hmi_data[4] + 5);
                    hmi_data += (hmi_data[4] + 5);
                } else if (hmi_data[1] == 0x15) {
                    hmi_size -= 4;
                    hmi_dbg += 4;
                    hmi_data += 4;
                }
                hmi_data += 4;
                hmi_size -= 4;
                hmi_dbg += 4;
            } else {
                if ((check_ret = check_midi_event(hmi_data, hmi_size, hmi_running_event, verbose, 0)) == -1) {
                    printf("Missing or Corrupt MIDI Data\n");
                    free (hmi_track_offset);
                    return -1;
                }

                // Running event
                // 0xff does not alter running event
                if ((*hmi_data == 0xF0) || (*hmi_data == 0xF7)) {
                    // Sysex resets running event data
                    hmi_running_event = 0;
                } else if (*hmi_data < 0xF0) {
                    // MIDI events 0x80 to 0xEF set running event
                    if (*hmi_data >= 0x80) {
                        hmi_running_event = *hmi_data;
                    }
                }
            //  if (verbose) printf("Running Event: 0x%.2x\n",hmi_running_event);

                if ((hmi_data[0] == 0xff) && (hmi_data[1] == 0x2f) && (hmi_data[2] == 0x00)) {
                    hmi_data += check_ret;
                    hmi_size -= check_ret;
                    hmi_dbg += check_ret;
                    break;
                }

                if ((hmi_running_event & 0xf0) == 0x90) {
                    // note on has extra data to specify how long the note is.
                    hmi_data += check_ret;
                    hmi_size -= check_ret;
                    hmi_dbg += check_ret;

                    hmi_delta = 0;
                    if (*hmi_data > 0x7f) {
                        while (*hmi_data > 0x7F) {
                            hmi_delta = (hmi_delta << 7) | (*hmi_data & 0x7F);
                            hmi_data++;
                            hmi_size--;
                            hmi_dbg++;
                        }
                    }
                    hmi_delta = (hmi_delta << 7) | (*hmi_data & 0x7F);
                    if (verbose) printf("Note Length: %f secs\n", ((float)hmi_delta * secs_per_tick));
                    hmi_data++;
                    hmi_size--;
                    hmi_dbg++;
                } else {
                    hmi_data += check_ret;
                    hmi_size -= check_ret;
                    hmi_dbg += check_ret;
                }
            }
        }
    }

    free (hmi_track_offset);
    return 0;
}

static int test_hmp(unsigned char * hmp_data, unsigned long int hmp_size, int verbose) {
    uint8_t is_hmq = 0;
    uint32_t zero_cnt = 0;
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t hmp_file_length = 0;
    uint32_t hmp_chunks = 0;
    uint32_t hmp_chunk_num = 0;
    uint32_t hmp_chunk_length = 0;
    uint32_t hmp_division = 0;
    uint32_t hmp_song_time = 0;
    uint32_t hmp_track = 0;
    uint32_t hmp_var_len_val = 0;
    int32_t check_ret = 0;

    // check the header
    if (strncmp((char *) hmp_data,"HMIMIDIP", 8) != 0) {
        printf("Not a valid HMP file: expected HMIMIDIP\n");
        return -1;
    }
    hmp_data += 8;
    hmp_size -= 8;

    if (strncmp((char *) hmp_data,"013195", 6) == 0) {
        is_hmq = 1;
        hmp_data += 6;
        hmp_size -= 6;
        if (verbose) printf("HMPv2 format detected\n");
    }

    // should be a bunch of \0's
    if (is_hmq) {
        zero_cnt = 18;
    } else {
        zero_cnt = 24;
    }
    for (i = 0; i < zero_cnt; i++) {
//      printf("DEBUG (%.2x): %.2x\n",i, hmp_data[i]);
        if (hmp_data[i] != 0) {
            printf("Not a valid HMP file\n");
            return -1;
        }
    }
    hmp_data += zero_cnt;
    hmp_size -= zero_cnt;

    hmp_file_length = *hmp_data++;
    hmp_file_length += (*hmp_data++ << 8);
    hmp_file_length += (*hmp_data++ << 16);
    hmp_file_length += (*hmp_data++ << 24);
    if (verbose) printf("File length: %u\n", hmp_file_length);
    // Next 12 bytes are normally \0 so skipping over them
    hmp_data += 12;
    hmp_size -= 16;

    hmp_chunks = *hmp_data++;
    hmp_chunks += (*hmp_data++ << 8);
    hmp_chunks += (*hmp_data++ << 16);
    hmp_chunks += (*hmp_data++ << 24);
    if (verbose) printf("Number of chunks: %u\n", hmp_chunks);
    // Unsure of what next 4 bytes are so skip over them
    hmp_data += 4;
    hmp_size -= 8;

    hmp_division = *hmp_data++;
    hmp_division += (*hmp_data++ << 8);
    hmp_division += (*hmp_data++ << 16);
    hmp_division += (*hmp_data++ << 24);
    if (verbose) {
        printf("Beats per minute: %u\n", hmp_division);
        set_secs_per_tick(60,(uint32_t)(60000000.0f / (float)hmp_division));
    }

    hmp_song_time = *hmp_data++;
    hmp_song_time += (*hmp_data++ << 8);
    hmp_song_time += (*hmp_data++ << 16);
    hmp_song_time += (*hmp_data++ << 24);
    hmp_size -= 8;
    if (verbose) printf("Song Time: %u\n", hmp_song_time);

    if (is_hmq) {
        hmp_data += 840;
        hmp_size -= 840;
    } else {
        hmp_data += 712;
        hmp_size -= 712;
    }
    for (i = 0; i < hmp_chunks; i++) {

        hmp_chunk_num = *hmp_data++;
        hmp_chunk_num += (*hmp_data++ << 8);
        hmp_chunk_num += (*hmp_data++ << 16);
        hmp_chunk_num += (*hmp_data++ << 24);
        hmp_size -= 4;
        if (verbose) printf("Chunk number: %u\n", hmp_chunk_num);

        hmp_chunk_length = *hmp_data++;
        hmp_chunk_length += (*hmp_data++ << 8);
        hmp_chunk_length += (*hmp_data++ << 16);
        hmp_chunk_length += (*hmp_data++ << 24);
        hmp_size -= 4;
        if (verbose) printf("Chunk length: %u\n", hmp_chunk_length);
        if (hmp_chunk_length > hmp_size) {
            printf("File too short\n");
            return -1;
        }

        hmp_track = *hmp_data++;
        hmp_track += (*hmp_data++ << 8);
        hmp_track += (*hmp_data++ << 16);
        hmp_track += (*hmp_data++ << 24);
        hmp_size -= 4;
        if (verbose) printf("Track Number: %u\n", hmp_track);

        // Start of Midi Data

        // because chunk length includes chunk header
        // remove header length from chunk length
        hmp_chunk_length -= 12;

        // Start of Midi Data
        for (j = 0; j < hmp_chunk_length; j++) {
            uint32_t var_len_shift = 0;
            hmp_var_len_val = 0;
            if (*hmp_data < 0x80) {
                do {
                    hmp_var_len_val = hmp_var_len_val | ((*hmp_data++ & 0x7F) << var_len_shift);
                    var_len_shift += 7;
                    hmp_size--;
                    j++;
                } while (*hmp_data < 0x80);
            }
            hmp_var_len_val = hmp_var_len_val | ((*hmp_data++ & 0x7F) << var_len_shift);
            hmp_size--;
            if (verbose) {
                add_and_display_time(hmp_var_len_val);
            }

            if ((check_ret = check_midi_event(hmp_data, hmp_size, 0, verbose, EVENT_DATA_8BIT)) == -1) {
                printf("Missing or Corrupt MIDI Data\n");
                return -1;
            }
            // Display loop start/end
            if ((hmp_chunk_num == 1) && ((hmp_data[0] & 0xf0) == 0xb0)) {
                if ((hmp_data[1] == 110) && (hmp_data[2] == 255) && (verbose)) printf("HMP Loop Start\n");
                if ((hmp_data[1] == 111) && (hmp_data[2] == 128) && (verbose)) printf("HMP Loop End\n");
            }
            j += check_ret;
            hmp_data += check_ret;
            hmp_size -= check_ret;
        }
    }

    return 0;
}

static int test_xmidi(unsigned char * xmidi_data, unsigned long int xmidi_size,
                      int verbose) {
    unsigned int tmp_val = 0;
    unsigned int i = 0;
    unsigned int j = 0;
    unsigned int form_cnt = 0;
    unsigned int cat_len = 0;
    unsigned int subform_len = 0;
    unsigned int event_len = 0;
    unsigned int divisions = 60;
    unsigned int tempo = 500000;
    uint32_t tempo_set = 0;

    if (strncmp((char *) xmidi_data,"FORM", 4) != 0) {
        printf("Not a valid xmidi file: expected FORM\n");
        return -1;
    }

    if (verbose)
        printf("First FORM found\n");

    xmidi_data += 4;
    xmidi_size -= 4;

    // bytes until next entry
    tmp_val = *xmidi_data++ << 24;
    tmp_val |= *xmidi_data++ << 16;
    tmp_val |= *xmidi_data++ << 8;
    tmp_val |= *xmidi_data++;
    xmidi_size -= 4;

    if (strncmp((char *) xmidi_data,"XDIRINFO", 8) != 0) {
        printf("Not a valid xmidi file: expected XDIRINFO\n");
        return -1;
    }
    xmidi_data += 8;
    xmidi_size -= 8;

    /*
    0x00 0x00 0x00 0x02 at this point are unknown
    so skip over them
    */
    xmidi_data += 4;
    xmidi_size -= 4;

    // number of forms contained after this point
    form_cnt = *xmidi_data++;

    if (verbose)
        printf("Contains %u forms\n", form_cnt);

    /*
      at this stage unsure if remaining data in
      this section means anything
     */
    tmp_val -= 13;
    xmidi_data += tmp_val;
    xmidi_size -= tmp_val;

    if (strncmp((char *) xmidi_data,"CAT ", 4) != 0) {
        printf("Not a valid xmidi file: expected CAT\n");
        return -1;
    }
    xmidi_data += 4;
    xmidi_size -= 4;

    // stored just in case it means something
    cat_len = *xmidi_data++ << 24;
    cat_len |= *xmidi_data++ << 16;
    cat_len |= *xmidi_data++ << 8;
    cat_len |= *xmidi_data++;
    xmidi_size -= 4;
    if (verbose)
        printf("CAT length = %u",cat_len);

    if (strncmp((char *) xmidi_data,"XMID", 4) != 0) {
        printf("Not a valid xmidi file: expected XMID\n");
        return -1;
    }
    xmidi_data += 4;
    xmidi_size -= 4;

    // Start of FORM data which contains the songs
    for (i = 0; i < form_cnt; i++) {
        if (strncmp((char *) xmidi_data,"FORM", 4) != 0) {
            printf("Not a valid xmidi file: expected FORM\n");
            return -1;
        }
        if (verbose)
            printf("\nNew FORM\n");
        xmidi_data += 4;
        xmidi_size -= 4;

        // stored just in case it means something
        subform_len = *xmidi_data++ << 24;
        subform_len |= *xmidi_data++ << 16;
        subform_len |= *xmidi_data++ << 8;
        subform_len |= *xmidi_data++;
        xmidi_size -= 4;
        if (verbose)
            printf("FORM length: %u\n",subform_len);

        if (strncmp((char *) xmidi_data,"XMID", 4) != 0) {
            printf("Not a valid xmidi file: expected XMID\n");
            return -1;
        }
        if (verbose)
            printf("XMID Data\n");
        xmidi_data += 4;
        xmidi_size -= 4;
        subform_len -= 4;

        if (verbose) {
            set_secs_per_tick (divisions, tempo);
        }

        do {
            if (strncmp((char *) xmidi_data,"TIMB", 4) == 0) {
            /*
                TODO: Do we need to explore this further
                */
                xmidi_data += 4;
                xmidi_size -= 4;

                tmp_val = *xmidi_data++ << 24;
                tmp_val |= *xmidi_data++ << 16;
                tmp_val |= *xmidi_data++ << 8;
                tmp_val |= *xmidi_data++;
                xmidi_size -= 4;
                subform_len -= 8;

                if (verbose)
                    printf("TIMB length: %u\n", tmp_val);

                /*
                    patch information
                */
                tmp_val /= 2;
                for (j=0; j < tmp_val; j++) {
                    if (verbose)
                        printf ("Patch:%i, Bank:%i\n", xmidi_data[0], xmidi_data[1]);
                    xmidi_data += 2;
                    xmidi_size -= 2;
                    subform_len -= 2;
                }
                if (verbose)
                    printf("\n");

            } else if (strncmp((char *) xmidi_data,"RBRN", 4) == 0) {
                xmidi_data += 4;
                xmidi_size -= 4;

                event_len = *xmidi_data++ << 24;
                event_len |= *xmidi_data++ << 16;
                event_len |= *xmidi_data++ << 8;
                event_len |= *xmidi_data++;
                xmidi_size -= 4;
                subform_len -= 8;

                if (verbose)
                    printf("RBRN length: %u\n",event_len);

                // TODO: still have to work out what this is.
                // Does not seem to be needed for midi playback.
                xmidi_data += event_len;
                subform_len -= event_len;

            } else if (strncmp((char *) xmidi_data,"EVNT", 4) == 0) {
                int check_ret = 0;

                xmidi_data += 4;
                xmidi_size -= 4;

                event_len = *xmidi_data++ << 24;
                event_len |= *xmidi_data++ << 16;
                event_len |= *xmidi_data++ << 8;
                event_len |= *xmidi_data++;
                xmidi_size -= 4;
                subform_len -= 8;

                if (verbose)
                    printf("EVENT length: %u\n",event_len);

                do {
                    if (*xmidi_data < 0x80) {
                        // Delta until next event?
                        tmp_val = 0;
                        tmp_val = (tmp_val << 7) | (*xmidi_data++ & 0x7F);
                        xmidi_size--;
                        event_len--;
                        subform_len--;
                        if (verbose) {
                            // printf ("Intervals: %u\n", tmp_val);
                            add_time(tmp_val);
                        }

                    } else {
                        if (verbose)
                            display_time();
                        if ((check_ret = check_midi_event(xmidi_data, xmidi_size, 0, verbose, 0)) == -1) {
                            printf("Missing or Corrupt MIDI Data\n");
                            return -1;
                        }
                        if ((*xmidi_data & 0xf0) == 0x90) {
                            xmidi_data += check_ret;
                            xmidi_size -= check_ret;
                            event_len -= check_ret;
                            subform_len -= check_ret;
                            tmp_val = 0;

                            if (*xmidi_data > 0x7f) {
                                while (*xmidi_data > 0x7f) {
                                    tmp_val = (tmp_val << 7) | (*xmidi_data++ & 0x7f);
                                    xmidi_size--;
                                    event_len--;
                                    subform_len--;
                                }
                            }
                            tmp_val = (tmp_val << 7) | (*xmidi_data++ & 0x7f);
                            xmidi_size--;
                            event_len--;
                            subform_len--;
                            if (verbose)
                                printf("Note Length: %f secs\n", ((float)tmp_val * secs_per_tick));
                        } else {
                            if ((xmidi_data[0] == 0xff) && (xmidi_data[1] == 0x51) && (xmidi_data[2] == 0x03)) {
                                if (!tempo_set) {
                                    tempo_set = 1;
                                    /* Tempo */
                                    tempo = (xmidi_data[3] << 16) + (xmidi_data[4] << 8)+ xmidi_data[5];
                                    if (!tempo)
                                        tempo = 500000;
                                    if (verbose) {
                                        set_secs_per_tick (divisions, tempo);
                                    }
                                }
                            }
                            xmidi_data += check_ret;
                            xmidi_size -= check_ret;
                            event_len -= check_ret;
                            subform_len -= check_ret;
                        }
                    }
                } while (event_len);
                if (verbose)
                    printf("\n");
            } else {
                printf("Not a valid xmidi file: unknown XMID entry\n");
                return -1;
            }
        } while (subform_len);
        if (verbose)
            printf("=============\n\n");
    }
    return 0;
}

static int test_midi(unsigned char * midi_data, unsigned long int midi_size,
                     int verbose) {
    unsigned int tmp_val;
    unsigned int track_size;
    unsigned char *next_track;
    unsigned int delta;
    unsigned long int delta_accum;
    unsigned int no_tracks;
    unsigned int i;
    unsigned int divisions = 96;
    unsigned char running_event = 0;
    unsigned long int tempo = 500000;
    int check_ret = 0;
    unsigned int total_count = 0;

    if (strncmp((char *) midi_data, "RIFF", 4) == 0) {
        midi_data += 20;
        midi_size -= 20;
        total_count += 20;
    }

    if (strncmp((char *) midi_data, "MThd", 4) != 0) {
        printf("Not a compatible file\n");
        return -1;
    }

    midi_data += 4;
    midi_size -= 4;
    total_count += 4;

    if (midi_size < 10) {
        printf("Midi File Too Short\n");
        return -1;
    }

    /*
     * Get Midi Header Size - must always be 6
     */
    tmp_val = *midi_data++ << 24;
    tmp_val |= *midi_data++ << 16;
    tmp_val |= *midi_data++ << 8;
    tmp_val |= *midi_data++;
    midi_size -= 4;
    total_count += 4;

    if (verbose)
        printf("Header Size: %i\n", tmp_val);

    if (tmp_val != 6) {
        printf("Corrupt Midi Header\n");
        return -1;
    }

    /*
     * Get Midi Format - we only support 0, 1 and 2
     */
    tmp_val = *midi_data++ << 8;
    tmp_val |= *midi_data++;
    midi_size -= 2;
    total_count += 2;

    if (verbose)
        printf("Format: %i\n", tmp_val);

    if (tmp_val > 2) {
        printf("Midi Format Not Supported\n");
        return -1;
    }

    /*
     * Get No. of Tracks
     */
    tmp_val = *midi_data++ << 8;
    tmp_val |= *midi_data++;
    midi_size -= 2;
    total_count += 2;

    if (verbose)
        printf("Number of Tracks: %i\n", tmp_val);

    if (tmp_val < 1) {
        printf("Midi Contains No Tracks\n");
        return -1;
    }
    no_tracks = tmp_val;

    /*
     * Get Divisions
     */
    divisions = *midi_data++ << 8;
    divisions |= *midi_data++;
    midi_size -= 2;
    total_count += 2;

    if (verbose) {
        printf("Divisions: %i\n", divisions);

        if (divisions & 0x00008000) {
            printf("Division Type Not Supported\n");
            return -1;
        }

        set_secs_per_tick (divisions, tempo);
    }

    for (i = 0; i < no_tracks; i++) {
        time_mins = 0;
        time_secs = 0.0;

        if (midi_size < 8) {
            printf("Midi File Too Short\n");
            return -1;
        }

        if (strncmp((char *) midi_data, "MTrk", 4) != 0) {
            printf("Expected Track Header\n");
            return -1;
        }

        if (verbose)
            printf("Start of Track\n");

        midi_data += 4;
        midi_size -= 4;
        total_count += 4;

        track_size = *midi_data++ << 24;
        track_size |= *midi_data++ << 16;
        track_size |= *midi_data++ << 8;
        track_size |= *midi_data++;
        midi_size -= 4;
        total_count += 4;
        if (verbose)
            printf("Track Size: %i\n", track_size);

        if (midi_size < track_size) {
            printf("Midi File Too Short: Missing Track Data\n");
            return -1;
        }
        if ((midi_data[track_size - 3] != 0xFF)
                || (midi_data[track_size - 2] != 0x2F)
                || (midi_data[track_size - 1] != 0x00)) {
            printf("Corrupt Midi, Expected EOT\n");
            return -1;
        }

        next_track = midi_data + track_size;
        delta_accum = 0;
        while (midi_data < next_track) {
            delta = 0;
//          printf("Get Delta: ");
            while (*midi_data > 0x7F) {
                delta = (delta << 7) | (*midi_data & 0x7F);
//              printf("0x%.2x ",*midi_data);
                midi_data++;
                midi_size--;
                total_count++;

                if (midi_size == 0) {
                    printf("Corrupt Midi, Missing or Corrupt Track Data\n");
                    return -1;
                }
            }
            delta = (delta << 7) | (*midi_data & 0x7F);
            if (verbose)
                add_and_display_time(delta);
            midi_data++;
            if (midi_size == 0) {
                printf("Corrupt Midi, Missing or Corrupt Track Data\n");
                return -1;
            }
            midi_size--;
            total_count++;
            delta_accum += delta;
            /* tempo microseconds per quarter note
             * divisions pulses per quarter note */
            /*if (verbose) printf("Est Seconds: %f\n",(((float)tempo/(float)divisions*(float)delta_accum)/1000000.0));*/
            /*
            if (verbose)
                printf("Delta: %i, Accumilated Delta: %ld\n", delta,
                        delta_accum);
            */
            if (*midi_data < 0x80) {
                if (running_event == 0) {
                    printf("Currupt Midi: expected event, got data\n");
                    return -1;
                }
            }

//          printf("Event Offset: 0x%.8x\n", total_count);
            if ((check_ret = check_midi_event(midi_data, midi_size, running_event, verbose, 0)) == -1) {
                printf("Missing or Corrupt MIDI Data 0x%.8x\n",total_count);
                return -1;
            }

            if ((*midi_data == 0xF0) || (*midi_data == 0xF7)) {
                /* Sysex resets running event data */
                running_event = 0;
            } else if (*midi_data < 0xF0) {
                /* MIDI events 0x80 to 0xEF set running event */
                if (*midi_data >= 0x80) {
                    running_event = *midi_data;
//                  printf("Set running_event 0x%2x\n", running_event);
                }
            }

            if ((midi_data[0] == 0xff) && (midi_data[1] == 0x51) && (midi_data[2] == 0x03)) {
                    /* Tempo */
                    tempo = (midi_data[3] << 16) + (midi_data[4] << 8)+ midi_data[5];
                    if (!tempo)
                        tempo = 500000;
                if (verbose) {
                    /* Slow but needed for accuracy */
                    set_secs_per_tick (divisions, tempo);
                }
            }

            midi_size -= check_ret;
            total_count += check_ret;

//          printf("Midi data remaining: %lu\n", midi_size);

            if (midi_size == 0) {
                /* check for end of track being at end */
                if ((midi_data[0] == 0xff) && (midi_data[1] == 0x2f) && (midi_data[2] == 0x0)) {
                    return 0;
                } else {
                    printf("Corrupt Midi, Missing or Corrupt Track Data\n");
                    return -1;
                }
            }
            midi_data += check_ret;

            if (midi_data > next_track) {
                printf("Corrupt Midi, Track Data went beyond track boundries.\n");
                return -1;
            }
        }
    }

    return 0;
}

static int test_guspat(unsigned char * gus_patch, unsigned long int filesize,
                       int verbose) {
    unsigned long int gus_ptr = 0;
    unsigned char no_of_samples = 0;

    if (filesize < 239) {
        printf("File too short\n");
        return -1;
    }

    if (memcmp(gus_patch, "GF1PATCH110\0ID#000002", 22)
            && memcmp(gus_patch, "GF1PATCH100\0ID#000002", 22)) {
        printf("Unsupported format\n");
        return -1;
    }

    if ((gus_patch[82] > 1) || (gus_patch[151] > 1)) {
        printf("Unsupported format\n");
        return -1;
    }

    no_of_samples = gus_patch[198];
    if (verbose)
        printf("Number of samples: %i\n", no_of_samples);
    gus_ptr = 239;
    do {
        if ((gus_ptr + 96) > filesize) {
            printf("File too short\n");
            return -1;
        }
        if (verbose) {
            printf("Sample Start\n");

            printf("Loop Fraction: 0x%x, ", gus_patch[gus_ptr + 7]);
            printf("Data Length: %i, ",
                    ((gus_patch[gus_ptr + 11] << 24) |
                     (gus_patch[gus_ptr + 10] << 16) |
                     (gus_patch[gus_ptr +  9] <<  8) |
                      gus_patch[gus_ptr +  8]));
            printf("Loop Start: %i, ",
                    ((gus_patch[gus_ptr + 15] << 24) |
                     (gus_patch[gus_ptr + 14] << 16) |
                     (gus_patch[gus_ptr + 13] <<  8) |
                      gus_patch[gus_ptr + 12]));
            printf("Loop End: %i\n",
                    ((gus_patch[gus_ptr + 19] << 24) |
                     (gus_patch[gus_ptr + 18] << 16) |
                     (gus_patch[gus_ptr + 17] <<  8) |
                      gus_patch[gus_ptr + 16]));

            printf("Rate: %i, ",
                    ((gus_patch[gus_ptr + 21] << 8) | gus_patch[gus_ptr + 20]));
            printf("Low Freq: %fHz, ",
                    (float) ((gus_patch[gus_ptr + 25] << 24) |
                             (gus_patch[gus_ptr + 24] << 16) |
                             (gus_patch[gus_ptr + 23] <<  8) |
                              gus_patch[gus_ptr + 22]) / 1000.0);
            printf("High Freq: %fHz, ",
                    (float) ((gus_patch[gus_ptr + 29] << 24) |
                             (gus_patch[gus_ptr + 28] << 16) |
                             (gus_patch[gus_ptr + 27] <<  8) |
                              gus_patch[gus_ptr + 26]) / 1000.0);
            printf("Root Freq: %fHz\n",
                    (float) ((gus_patch[gus_ptr + 33] << 24) |
                             (gus_patch[gus_ptr + 32] << 16) |
                             (gus_patch[gus_ptr + 31] <<  8) |
                              gus_patch[gus_ptr + 30]) / 1000.0);

            printf("Attack Level: %i, Attack Time: %fsecs\n",
                    gus_patch[gus_ptr + 43],
                    env_time_table[gus_patch[gus_ptr + 37]]);
            printf("Decay Level: %i, Decay Time: %fsecs\n",
                    gus_patch[gus_ptr + 44],
                    env_time_table[gus_patch[gus_ptr + 38]]);
            printf("Sustain Level: %i, Sustain Time: %fsecs\n",
                    gus_patch[gus_ptr + 45],
                    env_time_table[gus_patch[gus_ptr + 39]]);
            printf("Sustained Release Level: %i, Sustained Release Time: %fsecs\n",
                    gus_patch[gus_ptr + 46],
                    env_time_table[gus_patch[gus_ptr + 40]]);
            printf("Normal Release Level: %i, Normal Release Time: %fsecs\n",
                    gus_patch[gus_ptr + 47],
                    env_time_table[gus_patch[gus_ptr + 41]]);
            printf("Clamped Release Level: %i, Clamped Release Time: %fsecs\n",
                    gus_patch[gus_ptr + 48],
                    env_time_table[gus_patch[gus_ptr + 42]]);
        }

        if (env_time_table[gus_patch[gus_ptr + 40]]
                < env_time_table[gus_patch[gus_ptr + 41]]) {
            printf("WARNING!! Normal release envelope longer than sustained release envelope\n");
            printf("          Caused by patch editor not following the file format set by Gravis\n");
            printf("          WildMIDI will attempt to compensate for it on playback\n");
        }
        if (env_time_table[gus_patch[gus_ptr + 41]]
            < env_time_table[gus_patch[gus_ptr + 42]]) {
            printf("WARNING!! Clamped release envelope longer than normal release envelope\n");
            printf("          Caused by patch editor not following the file format set by Gravis\n");
            printf("          WildMIDI will attempt to compensate for it on playback\n");
        }

        if (verbose) {
            printf("Modes: ");
            if (gus_patch[gus_ptr + 55] & 0x01)
                printf("16 Bit  ");
            if (gus_patch[gus_ptr + 55] & 0x02)
                printf("Unsigned  ");
            if (gus_patch[gus_ptr + 55] & 0x04)
                printf("Loop  ");
            if (gus_patch[gus_ptr + 55] & 0x08)
                printf("Ping Pong  ");
            if (gus_patch[gus_ptr + 55] & 0x10)
                printf("Reverse  ");
            if (gus_patch[gus_ptr + 55] & 0x20)
                printf("Sustain  ");
            if (gus_patch[gus_ptr + 55] & 0x40)
                printf("Envelope  ");
            if (gus_patch[gus_ptr + 55] & 0x80)
                printf("Clamped Release  ");
            printf("\n");

            printf("Sample End\n\n");
        }
        gus_ptr +=
                     96
                        + ((gus_patch[gus_ptr + 11] << 24) |
                           (gus_patch[gus_ptr + 10] << 16) |
                           (gus_patch[gus_ptr +  9] <<  8) |
                            gus_patch[gus_ptr +  8]);
    } while (--no_of_samples);

    return 0;
}

int main(int argc, char ** argv) {
    int i;
    int option_index = 0;
    int verbose = 0;
    int testret = 0;
    uint8_t mus_hdr[] = { 'M', 'U', 'S', 0x1A };
    uint8_t xmi_hdr[] = { 'F', 'O', 'R', 'M' };
    int notes_still_on = 0;

    unsigned char *filebuffer = NULL;
    unsigned long int filesize = 0;

    do_version();
    while (1) {
        i = getopt_long(argc, argv, "d:f:vh", long_options, &option_index);
        if (i == -1)
            break;
        switch (i) {
        case 'd': /* Verbose */
            verbose = atoi(optarg);
            break;
        case 'f': /* Frequency */
            frequency = atof(optarg);
            break;
        case 'v': /* Version */
            return 0;
        case 'h': /* help */
            do_help();
            return 0;
        default:
            printf("Unknown Option -%o ??\n", i);
            return 0;
        }
    }
    if (optind >= argc) {
        return 0;
    }

    while (optind < argc) {
        printf("Testing: %s\n", argv[optind]);
        zero_check_notes();
        if ((filebuffer = DT_BufferFile(argv[optind], &filesize)) != NULL) {
            if (memcmp(filebuffer,"HMIMIDIP", 8) == 0) {
                testret = test_hmp(filebuffer, filesize, verbose);
                notes_still_on = count_check_notes();
            } else if (memcmp(filebuffer, "HMI-MIDISONG061595", 18) == 0) {
                testret = test_hmi(filebuffer, filesize, verbose);
                notes_still_on = count_check_notes();
            } else if (memcmp(filebuffer, mus_hdr, 4) == 0) {
                testret = test_mus(filebuffer, filesize, verbose);
                notes_still_on = count_check_notes();
            } else if (memcmp(filebuffer, xmi_hdr, 4) == 0) {
                testret = test_xmidi(filebuffer, filesize, verbose);
                notes_still_on = count_check_notes();
            } else  if ((memcmp(filebuffer, "GF1PATCH110\0ID#000002", 22) == 0) ||
                        (memcmp(filebuffer, "GF1PATCH100\0ID#000002", 22) == 0)) {
                testret = test_guspat(filebuffer, filesize, verbose);
            } else {
                testret = test_midi(filebuffer, filesize, verbose);
                notes_still_on = count_check_notes();
            }
            free(filebuffer);
            if (notes_still_on) {
                printf("%i notes still on after end of file\n",notes_still_on);
            }
            if (testret != 0) {
                printf("FAILED: %s will not work correctly with WildMIDI\n\n",
                        argv[optind]);
            } else {
                printf("Success\n\n");
            }
        }

        optind++;
    }

    return 0;
}
