/*
	DevTest_GusPat.c

 	Display Information about the Gravis Ultrasound patch file.

    NOTE: This file is intended for developer use to aide in feature development, and bug hunting.

    Copyright (C) Chris Ison 2001-2010

    This file is part of WildMIDI.

    WildMIDI is free software: you can redistribute and/or modify the library
    under the terms of the GNU Lesser General Public License and you can
    redistribute the player and DevTest under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the licenses, or(at your option) any later version.

    WildMIDI is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
    the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License and the
    GNU Lesser General Public License along with WildMIDI.  If not,  see
    <http://www.gnu.org/licenses/>.

    Email: wildcode@users.sourceforge.netchar
*/

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef _WIN32
# include <pwd.h>
#endif

#include "config.h"

static struct option const long_options[] = {
	{"debug-level",1,0,'d'},
	{"version",0,0,'v'},
	{"help",0,0,'h'},
	{NULL,0,NULL,0}
};


static float env_time_table[] = {
    0.0, 0.091728000, 0.045864000, 0.030576000, 0.022932000, 0.018345600, 0.015288000, 0.013104000, 0.011466000, 0.010192000, 0.009172800, 0.008338909, 0.007644000, 0.007056000, 0.006552000, 0.006115200, 0.005733000, 0.005395765, 0.005096000, 0.004827789, 0.004586400, 0.004368000, 0.004169455, 0.003988174, 0.003822000, 0.003669120, 0.003528000, 0.003397333, 0.003276000, 0.003163034, 0.003057600, 0.002958968, 0.002866500, 0.002779636, 0.002697882, 0.002620800, 0.002548000, 0.002479135, 0.002413895, 0.002352000, 0.002293200, 0.002237268, 0.002184000, 0.002133209, 0.002084727, 0.002038400, 0.001994087, 0.001951660, 0.001911000, 0.001872000, 0.001834560, 0.001798588, 0.001764000, 0.001730717, 0.001698667, 0.001667782, 0.001638000, 0.001609263, 0.001581517, 0.001554712, 0.001528800, 0.001503738, 0.001479484, 0.001456000,
    0.0, 0.733824000, 0.366912000, 0.244608000, 0.183456000, 0.146764800, 0.122304000, 0.104832000, 0.091728000, 0.081536000, 0.073382400, 0.066711273, 0.061152000, 0.056448000, 0.052416000, 0.048921600, 0.045864000, 0.043166118, 0.040768000, 0.038622316, 0.036691200, 0.034944000, 0.033355636, 0.031905391, 0.030576000, 0.029352960, 0.028224000, 0.027178667, 0.026208000, 0.025304276, 0.024460800, 0.023671742, 0.022932000, 0.022237091, 0.021583059, 0.020966400, 0.020384000, 0.019833081, 0.019311158, 0.018816000, 0.018345600, 0.017898146, 0.017472000, 0.017065674, 0.016677818, 0.016307200, 0.015952696, 0.015613277, 0.015288000, 0.014976000, 0.014676480, 0.014388706, 0.014112000, 0.013845736, 0.013589333, 0.013342255, 0.013104000, 0.012874105, 0.012652138, 0.012437695, 0.012230400, 0.012029902, 0.011835871, 0.011648000,
    0.0, 5.870592000, 2.935296000, 1.956864000, 1.467648000, 1.174118400, 0.978432000, 0.838656000, 0.733824000, 0.652288000, 0.587059200, 0.533690182, 0.489216000, 0.451584000, 0.419328000, 0.391372800, 0.366912000, 0.345328941, 0.326144000, 0.308978526, 0.293529600, 0.279552000, 0.266845091, 0.255243130, 0.244608000, 0.234823680, 0.225792000, 0.217429333, 0.209664000, 0.202434207, 0.195686400, 0.189373935, 0.183456000, 0.177896727, 0.172664471, 0.167731200, 0.163072000, 0.158664649, 0.154489263, 0.150528000, 0.146764800, 0.143185171, 0.139776000, 0.136525395, 0.133422545, 0.130457600, 0.127621565, 0.124906213, 0.122304000, 0.119808000, 0.117411840, 0.115109647, 0.112896000, 0.110765887, 0.108714667, 0.106738036, 0.104832000, 0.102992842, 0.101217103, 0.099501559, 0.097843200, 0.096239213, 0.094686968, 0.093184000,
    0.0, 46.964736000, 23.482368000, 15.654912000, 11.741184000, 9.392947200, 7.827456000, 6.709248000, 5.870592000, 5.218304000, 4.696473600, 4.269521455, 3.913728000, 3.612672000, 3.354624000, 3.130982400, 2.935296000, 2.762631529, 2.609152000, 2.471828211, 2.348236800, 2.236416000, 2.134760727, 2.041945043, 1.956864000, 1.878589440, 1.806336000, 1.739434667, 1.677312000, 1.619473655, 1.565491200, 1.514991484, 1.467648000, 1.423173818, 1.381315765, 1.341849600, 1.304576000, 1.269317189, 1.235914105, 1.204224000, 1.174118400, 1.145481366, 1.118208000, 1.092203163, 1.067380364, 1.043660800, 1.020972522, 0.999249702, 0.978432000, 0.958464000, 0.939294720, 0.920877176, 0.903168000, 0.886127094, 0.869717333, 0.853904291, 0.838656000, 0.823942737, 0.809736828, 0.796012475, 0.782745600, 0.769913705, 0.757495742, 0.745472000
};

void
do_version (void) {
    printf("DevTest for WildMIDI %s - For testing purposes only\n\n",PACKAGE_VERSION);
	printf("Copyright (C) Chris Ison 2001-2010 wildcode@users.sourceforge.net\n\n");
	printf("DevTest comes with ABSOLUTELY NO WARRANTY\n");
	printf("This is free software, and you are welcome to redistribute it\n");
	printf("under the terms and conditions of the GNU General Public License version 3.\n");
	printf("For more information see COPYING\n\n");
	printf("Report bugs to %s\n",PACKAGE_BUGREPORT);
	printf("WildMIDI homepage at %s\n",PACKAGE_URL);
    printf("\n");
}

void
do_help (void) {
    do_version();
    printf(" -d N   --debug-level N    Verbose output\n");
    printf(" -h     --help             Display this information\n");
    printf(" -v     --version          Display version information\n\n");
}

unsigned char *
DT_BufferFile (const char *filename, unsigned long int *size) {
	int buffer_fd;
	unsigned char *data;
	char *ret_data = NULL;
	struct stat buffer_stat;
#ifndef _WIN32
	char *home = NULL;
	struct passwd *pwd_ent;
	char buffer_dir[1024];
#endif

	char *buffer_file = malloc(strlen(filename) + 1);

	if (buffer_file == NULL) {
		printf("Unable to get ram to expand %s: %s\n", filename, strerror(errno));
		return NULL;
	}

	strcpy (buffer_file, filename);
#ifndef _WIN32
	if (strncmp(buffer_file,"~/",2) == 0) {
		if ((pwd_ent = getpwuid (getuid ()))) {
			home = pwd_ent->pw_dir;
		} else {
			home = getenv ("HOME");
		}
		if (home) {
			buffer_file = realloc(buffer_file,(strlen(buffer_file) + strlen(home) + 1));
			if (buffer_file == NULL) {
                printf("Unable to get ram to expand %s: %s\n", filename, strerror(errno));
				free(buffer_file);
				return NULL;
			}
			memmove((buffer_file + strlen(home)), (buffer_file + 1), (strlen(buffer_file)));
			strncpy (buffer_file, home,strlen(home));
		}
	} else if (buffer_file[0] != '/') {
		ret_data = getcwd(buffer_dir,1024);
		if (buffer_dir[strlen(buffer_dir)-1] != '/') {
			buffer_dir[strlen(buffer_dir)+1] = '\0';
			buffer_dir[strlen(buffer_dir)] = '/';
		}
		buffer_file = realloc(buffer_file,(strlen(buffer_file) + strlen(buffer_dir) + 1));
		if (buffer_file == NULL) {
            printf("Unable to get ram to expand %s: %s\n", filename, strerror(errno));
			free(buffer_file);
			return NULL;
		}
		memmove((buffer_file + strlen(buffer_dir)), buffer_file, strlen(buffer_file)+1);
		strncpy (buffer_file,buffer_dir,strlen(buffer_dir));
	}
#endif
	if (stat(buffer_file,&buffer_stat)) {
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
	if ((buffer_fd = open(buffer_file,O_RDONLY)) == -1) {
#endif
		printf("Unable to open %s: %s\n", filename, strerror(errno));
		free(buffer_file);
		free(data);
		return NULL;
	}
	if (read(buffer_fd,data,*size) != buffer_stat.st_size) {
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


int
test_midi(unsigned char * filebuffer, unsigned long int filesize, unsigned int verbose) {
    return 0;
}

int
test_guspat(unsigned char * gus_patch, unsigned long int filesize, unsigned int verbose) {
    unsigned long int gus_ptr = 0;
    unsigned char no_of_samples = 0;

    unsigned long int tmp_lint = 0;
    unsigned short int tmp_sint = 0;
    unsigned char tmp_char = 0;

    unsigned int i = 0;

    if (filesize < 239) {
        printf("File too short\n");
        return -1;
    }

   	if (memcmp(gus_patch, "GF1PATCH110\0ID#000002", 22) && memcmp(gus_patch, "GF1PATCH100\0ID#000002", 22)) {
        printf("Unsupported format\n");
        return -1;
	}

	if ((gus_patch[82] > 1) || (gus_patch[151] > 1)) {
        printf("Unsupported format\n");
        return -1;
	}

	no_of_samples = gus_patch[198];
    if (verbose) printf("Number of samples: %i\n", no_of_samples);
	gus_ptr = 239;
	do {
	    if ((gus_ptr + 96) > filesize) {
            printf("File too short\n");
            return -1;
	    }
	    if (verbose) {
            printf("Sample Start\n");

            printf("Loop Fraction: 0x%x, ", gus_patch[gus_ptr+7]);
            printf("Data Length: %i, ", ((gus_patch[gus_ptr+11] << 24) | (gus_patch[gus_ptr+10] << 16) | (gus_patch[gus_ptr+9] << 8) | gus_patch[gus_ptr+8]));
            printf("Loop Start: %i, ", ((gus_patch[gus_ptr+15] << 24) | (gus_patch[gus_ptr+14] << 16) | (gus_patch[gus_ptr+13] << 8) | gus_patch[gus_ptr+12]));
            printf("Loop End: %i\n", ((gus_patch[gus_ptr+19] << 24) | (gus_patch[gus_ptr+18] << 16) | (gus_patch[gus_ptr+17] << 8) | gus_patch[gus_ptr+16]));

            printf("Rate: %i, ", ((gus_patch[gus_ptr+21] << 8) | gus_patch[gus_ptr+20]));
            printf("Low Freq: %fHz, ", (float)((gus_patch[gus_ptr+25] << 24) | (gus_patch[gus_ptr+24] << 16) | (gus_patch[gus_ptr+23] << 8) | gus_patch[gus_ptr+22]) / 1000.0);
            printf("High Freq: %fHz, ", (float)((gus_patch[gus_ptr+29] << 24) | (gus_patch[gus_ptr+28] << 16) | (gus_patch[gus_ptr+27] << 8) | gus_patch[gus_ptr+26]) / 1000.0);
            printf("Root Freq: %fHz\n", (float)((gus_patch[gus_ptr+33] << 24) | (gus_patch[gus_ptr+32] << 16) | (gus_patch[gus_ptr+31] << 8) | gus_patch[gus_ptr+30]) / 1000.0);

            printf("Attack Level: %i, Attack Time: %fsecs\n", gus_patch[gus_ptr+43], env_time_table[gus_patch[gus_ptr+37]]);
            printf("Decay Level: %i, Decay Time: %fsecs\n", gus_patch[gus_ptr+44], env_time_table[gus_patch[gus_ptr+38]]);
            printf("Sustain Level: %i, Sustain Time: %fsecs\n", gus_patch[gus_ptr+45], env_time_table[gus_patch[gus_ptr+39]]);
            printf("Sustained Release Level: %i, Sustained Release Time: %fsecs\n", gus_patch[gus_ptr+46], env_time_table[gus_patch[gus_ptr+40]]);
            printf("Normal Release Level: %i, Normal Release Time: %fsecs\n", gus_patch[gus_ptr+47], env_time_table[gus_patch[gus_ptr+41]]);
            printf("Clamped Release Level: %i, Clamped Release Time: %fsecs\n", gus_patch[gus_ptr+48], env_time_table[gus_patch[gus_ptr+42]]);

		}

        if (env_time_table[gus_patch[gus_ptr+40]] < env_time_table[gus_patch[gus_ptr+41]]) {
            printf("WARNING!! Normal release envelope longer than sustained release envelope\n");
            printf("          Caused by patch editor not following the file format set by Gravis\n");
            printf("          Add guspat_editor_author_cant_read_so_fix_release_time_for_me to top of wildmidi.cfg\n");
        }

        if (verbose) {
            printf("Modes: ");
            if (gus_patch[gus_ptr+55] & 0x01) printf("16 Bit  ");
            if (gus_patch[gus_ptr+55] & 0x01) printf("Unsigned  ");
            if (gus_patch[gus_ptr+55] & 0x01) printf("Loop  ");
            if (gus_patch[gus_ptr+55] & 0x01) printf("Ping Pong  ");
            if (gus_patch[gus_ptr+55] & 0x01) printf("Reverse  ");
            if (gus_patch[gus_ptr+55] & 0x01) printf("Sustain  ");
            if (gus_patch[gus_ptr+55] & 0x01) printf("Envelope  ");
            if (gus_patch[gus_ptr+55] & 0x01) printf("Clamped Release  ");
            printf("\n");

            printf("Sample End\n\n");
        }
        gus_ptr += 96 + ((gus_patch[gus_ptr+11] << 24) | (gus_patch[gus_ptr+10] << 16) | (gus_patch[gus_ptr+9] << 8) | gus_patch[gus_ptr+8]);
	} while (--no_of_samples);
    return 0;
}

int
main (int argc, char ** argv) {
	int i;
	int option_index = 0;
	unsigned int verbose = 0;
    int testret = 0;

    unsigned char *filebuffer = NULL;
    unsigned long int filesize = 0;

	do_version();
	while (1) {
		i = getopt_long (argc, argv, "d:vh", long_options, &option_index);
		if (i == -1)
			break;
		switch (i) {
		    case 'd': // Verbose
                verbose = atoi(optarg);
                break;
			case 'v': // Version
				return 0;
			case 'h': // help
				do_help();
				return 0;
			default:
				printf ("Unknown Option -%o ??\n", i);
				return 0;
		}
	}
    if (optind >= argc) {
        return 0;
    }

    while (optind < argc) {
        if ((strcasecmp((argv[optind] + strlen(argv[optind]) - 4),".mid") != 0) && (strcasecmp((argv[optind] + strlen(argv[optind]) - 4),".pat") != 0)) {
            printf("Testing of %s is not supported\n", argv[optind]);
            optind++;
            continue;
        }

        printf("Testing: %s\n", argv[optind]);
        testret = 0;
        if ((filebuffer = DT_BufferFile(argv[optind], &filesize)) != NULL) {
            if (strcasecmp((argv[optind] + strlen(argv[optind]) - 4),".mid") == 0) {
                testret = test_midi(filebuffer, filesize, verbose);
            } else if (strcasecmp((argv[optind] + strlen(argv[optind]) - 4),".pat") == 0) {
                testret = test_guspat(filebuffer, filesize, verbose);
            }
            free(filebuffer);
            if (testret != 0) {
                printf("FAILED: %s will not work correctly with WildMIDI\n\n", argv[optind]);
            } else {
                printf ("Success\n\n");
            }
        }

		optind++;
    }

    return 0;
}
