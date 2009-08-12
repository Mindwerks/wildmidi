/*
    dev_test.c Developer test program, not intended for general use.
    Copyright (C) 2001-2008 Chris Ison

    This file is part of WildMIDI.

    WildMIDI is free software: you can redistribute and/or modify the players
    under the terms of the GNU General Public License and you can redistribute
    and/or modify the library under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation, either version 3 of
   the licenses, or(at your option) any later version.

    WildMIDI is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
    the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License and the
    GNU Lesser General Public License along with WildMIDI.  If not,  see
    <http://www.gnu.org/licenses/>.
	
    Email: cisos@bigpond.net.au
    	 wildcode@users.sourceforge.net
 
    gcc -Wall -Werror -O3 -funroll-loops -ffast-math -pg -fexpensive-optimizations -fstrict-aliasing -fno-common -g -march=i686 -I ../include -o dev_test error.c file_io.c timidity_cfg.c dev_test.c
 
     $Id: dev_test.c,v 1.9 2008/05/23 13:22:31 wildcode Exp $
*/

#include <stdio.h>
#include <stdlib.h>

#define TEST_CFG 1
// #define TEST_PATCH 1
// #define TEST_FILEBUF 1
#define LOOP_COUNT 100

#ifdef TEST_CFG
#include "wildmidi_cfg.h"

int
test_wm_load_wildmidi_cfg( const char *wildmidi_cfg_file ) {
	struct _patchcfg *wildmidi_cfg = NULL;
	
	if ((wildmidi_cfg = WM_LoadConfig(wildmidi_cfg_file)) != NULL) {
		WM_FreePatchcfg(wildmidi_cfg);
	}
	
	return 0;
}

#endif

#ifdef TEST_PATCH
#include "guspat.h"

int
test_wm_load_guspat( const char *guspat_file ) {
//	int i = 0;
	struct _sample *test_struct =  NULL;
//	struct _sample *temp_struct =  NULL;
	
//	printf("====== %s ======\n", guspat_file);
	if ((test_struct = load_guspat(guspat_file)) == NULL) {
		return -1;
	}
/*	
	temp_struct = test_struct;
	while (temp_struct != NULL) {
		printf("count = %lu\n",temp_struct->count);
		printf("rate = %u\n",temp_struct->rate);
		printf("freq_low = %lu\n",temp_struct->freq_low);
		printf("freq_high = %lu\n",temp_struct->freq_high);
		printf("freq_root = %lu\n",temp_struct->freq_root);
		printf("modes = %u\n",temp_struct->modes);
		printf("loop_start = %lu\n",temp_struct->loop_start);
		printf("loop_end = %lu\n",temp_struct->loop_end);
		printf("loop_fraction = %u\n",temp_struct->loop_fraction);
		for (i = 0; i < 7; i++) {
			printf("env_rate[%i] = %lu\n",i,temp_struct->env_rate[i]);
			printf("env_target[%i] = %lu\n",i,temp_struct->env_target[i]);
		}
		printf("\n");
		temp_struct = temp_struct->next;
	}
*/
	free_sample(test_struct);
	return 0;
}

#endif

#if TEST_FILEBUF
#include "file_io.h"
/*
	Test WM_BufferFile
*/	
int
test_wm_bufferfile( void ) {
	const char test_file[] = "../docs/gpl.txt\0";
	unsigned long int test_size = 0;
	char *test_buffer =  NULL;
	
	if ((test_buffer = WM_BufferFile(test_file, &test_size)) == NULL)
    {
        return -1;
	}
	
	if ((test_buffer = realloc(test_buffer, (test_size + 1))) == NULL)
	{
		return -1;
	}

	test_buffer[test_size] = '\0';
	printf ("%s\n", test_buffer);
	free(test_buffer);

	return 0;
}

#endif

int
main (int argc, char **argv) {
	int i = 0;
	int j = 0;
	
	for (i=0; i < LOOP_COUNT; i++){
		for (j =1; j < argc; j++) {

#ifdef TEST_CFG
			if (test_wm_load_wildmidi_cfg(argv[j]) != 0) {
				printf("Test Failed\n");
				return -1;
			}
#endif

#ifdef TEST_PATCH
			if (test_wm_load_guspat(argv[j]) != 0) {
				printf("Test Failed\n");
				return -1;
			}
#endif

#ifdef TEST_FILEBUF
			if (test_wm_bufferfile() != 0) {
				printf("Test Failed\n");
				return -1;
			}
#endif
		}
	}
	
	return 0;
}
