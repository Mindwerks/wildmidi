/*
	wildmidi_lib.c

 	Midi Wavetable Processing library

    Copyright (C) 2001-2010 Chris Ison

    This file is part of WildMIDI.

    WildMIDI is free software: you can redistribute and/or modify the player
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

    Email: wildcode@users.sourceforge.net
*/

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#ifndef _WIN32
#include <pwd.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef _WIN32
# include <windows.h>
#endif
#include "config.h"
#include "wildmidi_lib.h"

/*
 * =========================
 * Global Data and Data Structs
 * =========================
 */

static int WM_Initialized = 0;
static signed short int WM_MasterVolume = 948;
static unsigned short int WM_SampleRate = 0;
static unsigned short int WM_MixerOptions = 0;

static char WM_Version[] = "WildMidi Processing Library " WILDMIDILIB_VERSION;


struct _rvb {
	/* filter data */
	signed long int l_buf_flt_in[6][2];
	signed long int l_buf_flt_out[6][2];
	signed long int r_buf_flt_in[6][2];
	signed long int r_buf_flt_out[6][2];
	signed long int coeff[6][5];
	/* buffer data */
	signed long int *l_buf;
	signed long int *r_buf;
	int l_buf_size;
	int r_buf_size;
	int l_out;
	int r_out;
	int l_sp_in[8];
	int r_sp_in[8];
	int l_in[4];
	int r_in[4];
	int gain;
};


struct _env {
	float time;
	float level;
	unsigned char set;
};

struct _sample {
	unsigned long int data_length;
	unsigned long int loop_start;
	unsigned long int loop_end;
	unsigned long int loop_size;
	unsigned char loop_fraction;
	unsigned short int rate;
	unsigned long int freq_low;
	unsigned long int freq_high;
	unsigned long int freq_root;
	unsigned char modes;
	unsigned long int env_rate[7];
	unsigned long int env_target[7];
	unsigned long int inc_div;
	signed short *data;
	signed short max_peek;
	signed short min_peek;
	signed long int peek_adjust;
	struct _sample *next;
};

struct _patch {
	unsigned short patchid;
	unsigned char loaded;
	char *filename;
	signed short int amp;
	unsigned char keep;
	unsigned char remove;
	struct _env env[6];
	unsigned char note;
	unsigned long int inuse_count;
	struct _sample *first_sample;
	struct _patch *next;
};

struct _patch *patch[128];

static int patch_lock;

struct _channel {
	unsigned char bank;
	struct _patch *patch;
	unsigned char hold;
	unsigned char volume;
	unsigned char pressure;
	unsigned char expression;
	signed char balance;
	signed char pan;
	signed short int left_adjust;
	signed short int right_adjust;
	signed short int pitch;
	signed short int pitch_range;
	signed long int pitch_adjust;
	unsigned short reg_data;
};

#define HOLD_OFF 0x02

struct _note {
	unsigned short noteid;
	unsigned char velocity;
	struct _patch *patch;
	struct _sample *sample;
	unsigned long int sample_pos;
	unsigned long int sample_inc;
	signed long int env_inc;
	unsigned char env;
	unsigned long int env_level;
	unsigned char modes;
	unsigned char hold;
	unsigned char active;
	struct _note *next;
	unsigned long int vol_lvl;
};

struct _miditrack {
	unsigned long int length;
	unsigned long int ptr;
	unsigned long int delta;
	unsigned char running_event;
	unsigned char EOT;
};

struct _mdi_patches {
	struct _patch *patch;
	struct _mdi_patch *next;
};

struct _event_data {
    unsigned char channel;
    unsigned long int data;
};

struct _mdi {
	int lock;
	unsigned long int samples_to_mix;
    struct _event *events;
    struct _event *current_event;
    unsigned long int event_count;

    unsigned short midi_master_vol;
	struct _WM_Info info;
	struct _WM_Info *tmp_info;
	struct _channel channel[16];
	struct _note *note[128];
	struct _note **last_note;
	struct _note note_table[2][16][128];

	struct _patch **patches;
	unsigned long int patch_count;
	signed short int amp;

// setup data for auto amp
	signed long int log_max_vol;
	signed long int lin_max_vol;

    struct _rvb *reverb;
};

struct _event {
        void (*do_event)(struct _mdi *mdi, struct _event_data *data);
        struct _event_data event_data;
        unsigned long int samples_to_next;
        unsigned long int samples_to_next_fixed;
};

/* Gauss Interpolation code adapted from code supplied by Eric. A. Welsh */
static double newt_coeffs[58][58];		/* for start/end of samples */
static double *gauss_table[(1<<10)] = {0};	/* don't need doubles */
//static int gauss_window[35] = {0};
static int gauss_n = 34;	/* do not set this value higher than 34 */
			/* 34 is as high as we can go before errors crop up */

static void init_gauss (void) {
	/* init gauss table */
	int n = 34;
	int m, i, k, n_half = (n>>1);
	int j;
	int sign;
	double ck;
	double x, x_inc, xz;
	double z[35];
	double *gptr;

	newt_coeffs[0][0] = 1;

	for (i = 0; i <= n; i++) {
		newt_coeffs[i][0] = 1;
		newt_coeffs[i][i] = 1;

		if (i > 1) {
			newt_coeffs[i][0] = newt_coeffs[i-1][0] / i;
			newt_coeffs[i][i] = newt_coeffs[i-1][0] / i;
		}

		for (j = 1; j < i; j++) {
			newt_coeffs[i][j] = newt_coeffs[i-1][j-1] + newt_coeffs[i-1][j];
			if (i > 1)
				newt_coeffs[i][j] /= i;
		}
		z[i] = i / (4*M_PI);
	}

	for (i = 0; i <= n; i++)
		for (j = 0, sign = pow(-1, i); j <= i; j++, sign *= -1)
			newt_coeffs[i][j] *= sign;


	x_inc = 1.0 / (1<<10);
	for (m = 0, x = 0.0; m < (1<<10); m++, x += x_inc) {
		xz = (x + n_half) / (4*M_PI);
		gptr = gauss_table[m] = realloc(gauss_table[m], (n+1)*sizeof(double));

		for (k = 0; k <= n; k++) {
			ck = 1.0;

			for (i = 0; i <= n; i++) {
				if (i == k)
					continue;

				ck *= (sin(xz - z[i])) / (sin(z[k] - z[i]));
    			}
			*gptr++ = ck;
		}
	}
}

static void free_gauss (void) {
    int m;
    for (m = 0; m < (1<<10); m++) {
        free (gauss_table[m]);
    }
}

/*
	reverb function
*/
inline void
reset_reverb (struct _rvb *rvb) {
	int i,j;
	for (i = 0; i < rvb->l_buf_size; i++) {
		rvb->l_buf[i] = 0;
	}
	for (i = 0; i < rvb->r_buf_size; i++) {
		rvb->r_buf[i] = 0;
	}
	for (i = 0; i < 6; i++) {
		for (j = 0; j < 2; j++) {
			rvb->l_buf_flt_in[i][j] = 0;
			rvb->l_buf_flt_out[i][j] = 0;
			rvb->r_buf_flt_in[i][j] = 0;
			rvb->r_buf_flt_out[i][j] = 0;
		}
	}
}

/*
	init_reverb

	=========================
	Engine Description

	8 reflective points around the room
	2 speaker positions
	1 listener position

	Sounds come from the speakers to all reflective points and to the listener.
	Sound comes from the reflective points to the listener.
	These sounds are combined, put through a filter that mimics surface absorbtion.
	The combined sounds are also sent to the reflective points on the opposite side.

*/

inline struct _rvb *
init_reverb(int rate) {
	struct _rvb *rtn_rvb = malloc(sizeof(struct _rvb));

	int i = 0;


	struct _coord {
       double x;
       double y;
	};

	struct _coord SPL = { 2.5, 5.0 }; // Left Speaker Position
	struct _coord SPR = { 7.5, 5.0 }; // Right Speaker Position
	struct _coord LSN = { 5.0, 15 }; // Listener Position
	// position of the reflective points
	struct _coord RFN[] = {
		{ 5.0, 0.0 },
		{ 0.0, 6.66666 },
		{ 0.0, 13.3333 },
		{ 5.0, 20.0 },
		{ 10.0, 20.0 },
		{ 15.0, 13.3333 },
		{ 15.0, 6.66666 },
		{ 10.0, 0.0 }
	};

	//distance
	double SPL_DST[8];
	double SPR_DST[8];
	double RFN_DST[8];

	double MAXL_DST = 0.0;
	double MAXR_DST = 0.0;

	double SPL_LSN_XOFS = SPL.x - LSN.x;
	double SPL_LSN_YOFS = SPL.y - LSN.y;
	double SPL_LSN_DST = sqrtf((SPL_LSN_XOFS * SPL_LSN_XOFS) + (SPL_LSN_YOFS * SPL_LSN_YOFS));

	double SPR_LSN_XOFS = SPR.x - LSN.x;
	double SPR_LSN_YOFS = SPR.y - LSN.y;
	double SPR_LSN_DST = sqrtf((SPR_LSN_XOFS * SPR_LSN_XOFS) + (SPR_LSN_YOFS * SPR_LSN_YOFS));


	if (rtn_rvb == NULL) {
		return NULL;
	}

	/*
		setup Peaking band EQ filter
		filter based on public domain code by Tom St Denis
		http://www.musicdsp.org/showone.php?id=64
	*/
	for (i = 0; i < 6; i++) {
		/*
			filters set at 125Hz, 250Hz, 500Hz, 1000Hz, 2000Hz, 4000Hz
		*/
		double Freq[] = {125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0};

		/*
			modify these to adjust the absorption qualities of the surface.
			Remember that lower frequencies are less effected by surfaces
		*/
		double dbAttn[] = {-0.0, -6.0, -13.0, -21.0, -30.0, -40.0};
		//double dbAttn[] = {-40.0, -30.0, -21.0, -13.0, -6.0, -0.0};
		double srate = (double)rate;
		double bandwidth = 2.0;
		double omega = 2.0 * M_PI * Freq[i] / srate;
		double sn = sin(omega);
		double cs = cos(omega);
		double alpha = sn * sinh(M_LN2 /2 * bandwidth * omega /sn);
		double A = pow(10.0, (dbAttn[i] / 40.0));
		/*
			Peaking band EQ filter
		*/
		double b0 = 1 + (alpha * A);
		double b1 = -2 * cs;
		double b2 = 1 - (alpha * A);
		double a0 = 1 + (alpha /A);
		double a1 = -2 * cs;
		double a2 = 1 - (alpha /A);

		rtn_rvb->coeff[i][0] = (signed long int)((b0 /a0) * 1024.0);
		rtn_rvb->coeff[i][1] = (signed long int)((b1 /a0) * 1024.0);
		rtn_rvb->coeff[i][2] = (signed long int)((b2 /a0) * 1024.0);
		rtn_rvb->coeff[i][3] = (signed long int)((a1 /a0) * 1024.0);
		rtn_rvb->coeff[i][4] = (signed long int)((a2 /a0) * 1024.0);
	}

	/* setup the reverb now */

	if (SPL_LSN_DST > MAXL_DST) MAXL_DST = SPL_LSN_DST;
	if (SPR_LSN_DST > MAXR_DST) MAXR_DST = SPR_LSN_DST;

	for (i = 0; i < 8; i++) {
		double SPL_RFL_XOFS = 0;
		double SPL_RFL_YOFS = 0;
		double SPR_RFL_XOFS = 0;
		double SPR_RFL_YOFS = 0;
		double RFN_XOFS = 0;
		double RFN_YOFS = 0;

		/* distance from listener to reflective surface */
		RFN_XOFS = LSN.x - RFN[i].x;
		RFN_YOFS = LSN.y - RFN[i].y;
		RFN_DST[i] = sqrtf((RFN_XOFS * RFN_XOFS) + (RFN_YOFS * RFN_YOFS));

		/* distance from speaker to 1st reflective surface */
		SPL_RFL_XOFS = SPL.x - RFN[i].x;
		SPL_RFL_YOFS = SPL.y - RFN[i].y;
		SPR_RFL_XOFS = SPR.x - RFN[i].x;
		SPR_RFL_YOFS = SPR.y - RFN[i].y;
		SPL_DST[i] = sqrtf((SPL_RFL_XOFS * SPL_RFL_XOFS) + (SPL_RFL_YOFS * SPL_RFL_YOFS));
		SPR_DST[i] = sqrtf((SPR_RFL_XOFS * SPR_RFL_XOFS) + (SPR_RFL_YOFS * SPR_RFL_YOFS));
		/*
			add the 2 distances together and remove the speaker to listener distance
			so we dont have to delay the initial output
		*/
		SPL_DST[i] += RFN_DST[i];

		SPL_DST[i] -= SPL_LSN_DST;

		if (i < 4) {
			if (SPL_DST[i] > MAXL_DST) MAXL_DST = SPL_DST[i];
		} else {
			if (SPL_DST[i] > MAXR_DST) MAXR_DST = SPL_DST[i];
		}

		SPR_DST[i] += RFN_DST[i];

		SPR_DST[i] -= SPR_LSN_DST;
		if (i < 4) {
			if (SPR_DST[i] > MAXL_DST) MAXL_DST = SPR_DST[i];
		} else {
			if (SPR_DST[i] > MAXR_DST) MAXR_DST = SPR_DST[i];
		}



		/*
			Double the reflection distance so that we get the full distance traveled
		*/
		RFN_DST[i] *= 2.0;

		if (i < 4) {
			if (RFN_DST[i] > MAXL_DST) MAXL_DST = RFN_DST[i];
		} else {
			if (RFN_DST[i] > MAXR_DST) MAXR_DST = RFN_DST[i];
		}
	}

	/* init the reverb buffers */
	rtn_rvb->l_buf_size = (int)((float)rate * (MAXL_DST / 340.29));
	rtn_rvb->l_buf = malloc(sizeof(signed long int) * (rtn_rvb->l_buf_size + 1));
	rtn_rvb->l_out = 0;

	rtn_rvb->r_buf_size = (int)((float)rate * (MAXR_DST / 340.29));
	rtn_rvb->r_buf = malloc(sizeof(signed long int) * (rtn_rvb->r_buf_size + 1));
	rtn_rvb->r_out = 0;

	for (i=0; i< 4; i++) {
		rtn_rvb->l_sp_in[i] = (int)((float)rate * (SPL_DST[i] / 340.29));
		rtn_rvb->l_sp_in[i+4] = (int)((float)rate * (SPL_DST[i+4] / 340.29));
		rtn_rvb->r_sp_in[i] = (int)((float)rate * (SPR_DST[i] / 340.29));
		rtn_rvb->r_sp_in[i+4] = (int)((float)rate * (SPR_DST[i+4] / 340.29));
		rtn_rvb->l_in[i] = (int)((float)rate * (RFN_DST[i] / 340.29));
		rtn_rvb->r_in[i] = (int)((float)rate * (RFN_DST[i+4] / 340.29));
	}

	rtn_rvb->gain = 4;

	reset_reverb(rtn_rvb);
	return rtn_rvb;
}

/*
	free_reverb - free up memory used for reverb
*/
inline void
free_reverb (struct _rvb *rvb) {
	free (rvb->l_buf);
	free (rvb->r_buf);
	free (rvb);
}


inline void
do_reverb (struct _rvb *rvb, signed long int *buffer, int size) {
	int i, j;
	signed long int l_buf_flt = 0;
	signed long int r_buf_flt = 0;
	signed long int l_rfl = 0;
	signed long int r_rfl = 0;
	int vol_div = 32;

	for (i = 0; i < size; i += 2) {
		signed long int tmp_l_val = 0;
		signed long int tmp_r_val = 0;
		/*
			add the initial reflections
			from each speaker, 4 to go the left, 4 go to the right buffers
		*/
		tmp_l_val = buffer[i] / vol_div;
		tmp_r_val = buffer[i + 1] / vol_div;
		for (j = 0; j < 4; j++) {
			rvb->l_buf[rvb->l_sp_in[j]] += tmp_l_val;
			rvb->l_sp_in[j] = (rvb->l_sp_in[j] + 1) % rvb->l_buf_size;
			rvb->l_buf[rvb->r_sp_in[j]] += tmp_r_val;
			rvb->r_sp_in[j] = (rvb->r_sp_in[j] + 1) % rvb->l_buf_size;

			rvb->r_buf[rvb->l_sp_in[j+4]] += tmp_l_val;
			rvb->l_sp_in[j+4] = (rvb->l_sp_in[j+4] + 1) % rvb->r_buf_size;
			rvb->r_buf[rvb->r_sp_in[j+4]] += tmp_r_val;
			rvb->r_sp_in[j+4] = (rvb->r_sp_in[j+4] + 1) % rvb->r_buf_size;
		}

		/*
			filter the reverb output and add to buffer
		*/
		l_rfl = rvb->l_buf[rvb->l_out];
		rvb->l_buf[rvb->l_out] = 0;
		rvb->l_out = (rvb->l_out + 1) % rvb->l_buf_size;

		r_rfl = rvb->r_buf[rvb->r_out];
		rvb->r_buf[rvb->r_out] = 0;
		rvb->r_out = (rvb->r_out + 1) % rvb->r_buf_size;

		for (j = 0; j < 6; j++) {
			l_buf_flt = ((l_rfl * rvb->coeff[j][0]) +
				(rvb->l_buf_flt_in[j][0] * rvb->coeff[j][1]) +
				(rvb->l_buf_flt_in[j][1] * rvb->coeff[j][2]) -
				(rvb->l_buf_flt_out[j][0] * rvb->coeff[j][3]) -
				(rvb->l_buf_flt_out[j][1] * rvb->coeff[j][4])) / 1024;
			rvb->l_buf_flt_in[j][1] = rvb->l_buf_flt_in[j][0];
			rvb->l_buf_flt_in[j][0] = l_rfl;
			rvb->l_buf_flt_out[j][1] = rvb->l_buf_flt_out[j][0];
			rvb->l_buf_flt_out[j][0] = l_buf_flt;
			buffer[i] += l_buf_flt;

			r_buf_flt = ((r_rfl * rvb->coeff[j][0]) +
				(rvb->r_buf_flt_in[j][0] * rvb->coeff[j][1]) +
				(rvb->r_buf_flt_in[j][1] * rvb->coeff[j][2]) -
				(rvb->r_buf_flt_out[j][0] * rvb->coeff[j][3]) -
				(rvb->r_buf_flt_out[j][1] * rvb->coeff[j][4])) / 1024;
			rvb->r_buf_flt_in[j][1] = rvb->r_buf_flt_in[j][0];
			rvb->r_buf_flt_in[j][0] = r_rfl;
			rvb->r_buf_flt_out[j][1] = rvb->r_buf_flt_out[j][0];
			rvb->r_buf_flt_out[j][0] = r_buf_flt;
			buffer[i+1] += r_buf_flt;
		}

		/*
			add filtered result back into the buffers but on the opposite side
		*/
		tmp_l_val = buffer[i+1] / vol_div;
		tmp_r_val = buffer[i] / vol_div;
		for (j = 0; j < 4; j++) {
			rvb->l_buf[rvb->l_in[j]] += tmp_l_val;
			rvb->l_in[j] = (rvb->l_in[j] + 1) % rvb->l_buf_size;

			rvb->r_buf[rvb->r_in[j]] += tmp_r_val;
			rvb->r_in[j] = (rvb->r_in[j] + 1) % rvb->r_buf_size;
		}
	}
	return;
}


struct _hndl {
	void * handle;
	struct _hndl *next;
	struct _hndl *prev;
};

struct _hndl * first_handle = NULL;

//f: ( VOLUME / 127 )
//f: pow(( VOLUME / 127 ), 1.660964047 )
//f: pow(( VOLUME / 127 ), 2.0 )
//f: pow(( VOLUME / 127 ), 0.602059991 )
//f: pow(( VOLUME / 127 ), 0.5 )

static signed short int lin_volume[] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 129, 137, 145, 153, 161, 169, 177, 185, 193, 201, 209, 217, 225, 233, 241, 249, 258, 266, 274, 282, 290, 298, 306, 314, 322, 330, 338, 346, 354, 362, 370, 378, 387, 395, 403, 411, 419, 427, 435, 443, 451, 459, 467, 475, 483, 491, 499, 507, 516, 524, 532, 540, 548, 556, 564, 572, 580, 588, 596, 604, 612, 620, 628, 636, 645, 653, 661, 669, 677, 685, 693, 701, 709, 717, 725, 733, 741, 749, 757, 765, 774, 782, 790, 798, 806, 814, 822, 830, 838, 846, 854, 862, 870, 878, 886, 894, 903, 911, 919, 927, 935, 943, 951, 959, 967, 975, 983, 991, 999, 1007, 1015, 1024 };
//static signed short int log_volume[] = { 0, 0, 1, 2, 3, 4, 6, 8, 10, 12, 15, 17, 20, 23, 26, 29, 32, 36, 39, 43, 47, 51, 55, 59, 64, 68, 73, 78, 83, 88, 93, 98, 103, 109, 114, 120, 126, 132, 138, 144, 150, 156, 162, 169, 176, 182, 189, 196, 203, 210, 217, 224, 232, 239, 247, 255, 262, 270, 278, 286, 294, 302, 311, 319, 328, 336, 345, 353, 362, 371, 380, 389, 398, 408, 417, 426, 436, 446, 455, 465, 475, 485, 495, 505, 515, 525, 535, 546, 556, 567, 577, 588, 599, 610, 621, 632, 643, 654, 665, 677, 688, 699, 711, 723, 734, 746, 758, 770, 782, 794, 806, 818, 831, 843, 855, 868, 880, 893, 906, 919, 931, 944, 957, 970, 984, 997, 1010, 1024 };
static signed short int sqr_volume[] = { 0, 0, 0, 0, 1, 1, 2, 3, 4, 5, 6, 7, 9, 10, 12, 14, 16, 18, 20, 22, 25, 27, 30, 33, 36, 39, 42, 46, 49, 53, 57, 61, 65, 69, 73, 77, 82, 86, 91, 96, 101, 106, 111, 117, 122, 128, 134, 140, 146, 152, 158, 165, 171, 178, 185, 192, 199, 206, 213, 221, 228, 236, 244, 251, 260, 268, 276, 284, 293, 302, 311, 320, 329, 338, 347, 357, 366, 376, 386, 396, 406, 416, 426, 437, 447, 458, 469, 480, 491, 502, 514, 525, 537, 549, 560, 572, 585, 597, 609, 622, 634, 647, 660, 673, 686, 699, 713, 726, 740, 754, 768, 782, 796, 810, 825, 839, 854, 869, 884, 899, 914, 929, 944, 960, 976, 992, 1007, 1024 };
//signed short int pan_volume[] = { 0, 55, 84, 107, 127, 146, 162, 178, 193, 208, 221, 234, 247, 259, 271, 282, 294, 305, 315, 326, 336, 346, 356, 366, 375, 384, 394, 403, 412, 420, 429, 438, 446, 454, 463, 471, 479, 487, 495, 503, 510, 518, 525, 533, 540, 548, 555, 562, 570, 577, 584, 591, 598, 605, 611, 618, 625, 632, 638, 645, 651, 658, 664, 671, 677, 684, 690, 696, 703, 709, 715, 721, 727, 733, 739, 745, 751, 757, 763, 769, 775, 781, 786, 792, 798, 804, 809, 815, 821, 826, 832, 837, 843, 848, 854, 859, 865, 870, 876, 881, 886, 892, 897, 902, 907, 913, 918, 923, 928, 933, 939, 944, 949, 954, 959, 964, 969, 974, 979, 984, 989, 994, 999, 1004, 1009, 1014, 1019, 1024 };
static signed short int pan_volume[] = { 0, 90, 128, 157, 181, 203, 222, 240, 257, 272, 287, 301, 314, 327, 339, 351, 363, 374, 385, 396, 406, 416, 426, 435, 445, 454, 463, 472, 480, 489, 497, 505, 514, 521, 529, 537, 545, 552, 560, 567, 574, 581, 588, 595, 602, 609, 616, 622, 629, 636, 642, 648, 655, 661, 667, 673, 679, 686, 692, 697, 703, 709, 715, 721, 726, 732, 738, 743, 749, 754, 760, 765, 771, 776, 781, 786, 792, 797, 802, 807, 812, 817, 822, 827, 832, 837, 842, 847, 852, 857, 862, 866, 871, 876, 880, 885, 890, 894, 899, 904, 908, 913, 917, 922, 926, 931, 935, 939, 944, 948, 953, 957, 961, 965, 970, 974, 978, 982, 987, 991, 995, 999, 1003, 1007, 1011, 1015, 1019, 1024 };

static float env_time_table[] = {
	0.0, 0.092857143, 0.046428571, 0.030952381, 0.023214286, 0.018571429, 0.015476190, 0.013265306, 0.011607143, 0.010317460, 0.009285714, 0.008441558, 0.007738095, 0.007142857, 0.006632653, 0.006190476, 0.005803571, 0.005462185, 0.005158730, 0.004887218, 0.004642857, 0.004421769, 0.004220779, 0.004037267, 0.003869048, 0.003714286, 0.003571429, 0.003439153, 0.003316327, 0.003201970, 0.003095238, 0.002995392, 0.002901786, 0.002813853, 0.002731092, 0.002653061, 0.002579365, 0.002509653, 0.002443609, 0.002380952, 0.002321429, 0.002264808, 0.002210884, 0.002159468, 0.002110390, 0.002063492, 0.002018634, 0.001975684, 0.001934524, 0.001895044, 0.001857143, 0.001820728, 0.001785714, 0.001752022, 0.001719577, 0.001688312, 0.001658163, 0.001629073, 0.001600985, 0.001573850, 0.001547619, 0.001522248, 0.001497696, 0.001473923,
	0.0, 0.742857143, 0.371428571, 0.247619048, 0.185714286, 0.148571429, 0.123809524, 0.106122449, 0.092857143, 0.082539683, 0.074285714, 0.067532468, 0.061904762, 0.057142857, 0.053061224, 0.049523810, 0.046428571, 0.043697479, 0.041269841, 0.039097744, 0.037142857, 0.035374150, 0.033766234, 0.032298137, 0.030952381, 0.029714286, 0.028571429, 0.027513228, 0.026530612, 0.025615764, 0.024761905, 0.023963134, 0.023214286, 0.022510823, 0.021848739, 0.021224490, 0.020634921, 0.020077220, 0.019548872, 0.019047619, 0.018571429, 0.018118467, 0.017687075, 0.017275748, 0.016883117, 0.016507937, 0.016149068, 0.015805471, 0.015476190, 0.015160350, 0.014857143, 0.014565826, 0.014285714, 0.014016173, 0.013756614, 0.013506494, 0.013265306, 0.013032581, 0.012807882, 0.012590799, 0.012380952, 0.012177986, 0.011981567, 0.011791383,
	0.0, 5.942857143, 2.971428571, 1.980952381, 1.485714286, 1.188571429, 0.990476190, 0.848979592, 0.742857143, 0.660317460, 0.594285714, 0.540259740, 0.495238095, 0.457142857, 0.424489796, 0.396190476, 0.371428571, 0.349579832, 0.330158730, 0.312781955, 0.297142857, 0.282993197, 0.270129870, 0.258385093, 0.247619048, 0.237714286, 0.228571429, 0.220105820, 0.212244898, 0.204926108, 0.198095238, 0.191705069, 0.185714286, 0.180086580, 0.174789916, 0.169795918, 0.165079365, 0.160617761, 0.156390977, 0.152380952, 0.148571429, 0.144947735, 0.141496599, 0.138205980, 0.135064935, 0.132063492, 0.129192547, 0.126443769, 0.123809524, 0.121282799, 0.118857143, 0.116526611, 0.114285714, 0.112129380, 0.110052910, 0.108051948, 0.106122449, 0.104260652, 0.102463054, 0.100726392, 0.099047619, 0.097423888, 0.095852535, 0.094331066,
	0.0, 47.542857143, 23.771428571, 15.847619048, 11.885714286, 9.508571429, 7.923809524, 6.791836735, 5.942857143, 5.282539683, 4.754285714, 4.322077922, 3.961904762, 3.657142857, 3.395918367, 3.169523810, 2.971428571, 2.796638655, 2.641269841, 2.502255639, 2.377142857, 2.263945578, 2.161038961, 2.067080745, 1.980952381, 1.901714286, 1.828571429, 1.760846561, 1.697959184, 1.639408867, 1.584761905, 1.533640553, 1.485714286, 1.440692641, 1.398319328, 1.358367347, 1.320634921, 1.284942085, 1.251127820, 1.219047619, 1.188571429, 1.159581882, 1.131972789, 1.105647841, 1.080519481, 1.056507937, 1.033540373, 1.011550152, 0.990476190, 0.970262391, 0.950857143, 0.932212885, 0.914285714, 0.897035040, 0.880423280, 0.864415584, 0.848979592, 0.834085213, 0.819704433, 0.805811138, 0.792380952, 0.779391101, 0.766820276, 0.754648526
};

static unsigned long int freq_table[] = {
	837201792, 837685632, 838169728, 838653568, 839138240, 839623232, 840108480, 840593984, 841079680, 841565184, 842051648, 842538240, 843025152, 843512320, 843999232, 844486976, 844975040, 845463360, 845951936, 846440320, 846929536, 847418944, 847908608, 848398656, 848888960, 849378944, 849869824, 850361024, 850852416, 851344192, 851835584, 852327872, 852820480, 853313280, 853806464, 854299328, 854793024, 855287040, 855781312, 856275904, 856770752, 857265344, 857760704, 858256448, 858752448, 859248704, 859744768, 860241600, 860738752, 861236160, 861733888, 862231360, 862729600, 863228160, 863727104, 864226176, 864725696, 865224896, 865724864, 866225152, 866725760, 867226688, 867727296, 868228736, 868730496, 869232576, 869734912, 870236928, 870739904, 871243072, 871746560, 872250368, 872754496, 873258240, 873762880, 874267840, 874773184, 875278720, 875783936, 876290112, 876796480, 877303232, 877810176, 878317504, 878824512, 879332416, 879840576, 880349056, 880857792, 881366272, 881875712, 882385280, 882895296, 883405440, 883915456, 884426304, 884937408, 885448832, 885960512, 886472512,
	886984192, 887496768, 888009728, 888522944, 889036352, 889549632, 890063680, 890578048, 891092736, 891607680, 892122368, 892637952, 893153792, 893670016, 894186496, 894703232, 895219648, 895737024, 896254720, 896772672, 897290880, 897808896, 898327744, 898846912, 899366336, 899886144, 900405568, 900925952, 901446592, 901967552, 902488768, 903010368, 903531584, 904053760, 904576256, 905099008, 905622016, 906144896, 906668480, 907192512, 907716800, 908241408, 908765632, 909290816, 909816256, 910342144, 910868160, 911394624, 911920768, 912447680, 912975104, 913502720, 914030592, 914558208, 915086784, 915615552, 916144768, 916674176, 917203968, 917733440, 918263744, 918794496, 919325440, 919856704, 920387712, 920919616, 921451840, 921984320, 922517184, 923049728, 923583168, 924116928, 924651008, 925185344, 925720000, 926254336, 926789696, 927325312, 927861120, 928397440, 928933376, 929470208, 930007296, 930544768, 931082560, 931619968, 932158464, 932697152, 933236160, 933775488, 934315072, 934854464, 935394688, 935935296, 936476224, 937017344, 937558208, 938100160, 938642304, 939184640,
	939727488, 940269888, 940813312, 941357056, 941900992, 942445440, 942990016, 943534400, 944079680, 944625280, 945171200, 945717440, 946263360, 946810176, 947357376, 947904832, 948452672, 949000192, 949548608, 950097280, 950646400, 951195776, 951745472, 952294912, 952845184, 953395904, 953946880, 954498176, 955049216, 955601088, 956153408, 956705920, 957258816, 957812032, 958364928, 958918848, 959472960, 960027456, 960582272, 961136768, 961692224, 962248000, 962804032, 963360448, 963916608, 964473600, 965031040, 965588736, 966146816, 966705152, 967263168, 967822144, 968381440, 968941120, 969501056, 970060736, 970621376, 971182272, 971743488, 972305088, 972866368, 973428608, 973991104, 974554048, 975117312, 975680768, 976243968, 976808192, 977372736, 977937536, 978502656, 979067584, 979633344, 980199488, 980765888, 981332736, 981899200, 982466688, 983034432, 983602624, 984171008, 984739776, 985308160, 985877632, 986447360, 987017472, 987587904, 988157952, 988729088, 989300416, 989872192, 990444224, 991016000, 991588672, 992161728, 992735168, 993308864, 993882880, 994456576, 995031296,
	995606336, 996181696, 996757440, 997332800, 997909184, 998485888, 999062912, 999640256, 1000217984, 1000795392, 1001373696, 1001952448, 1002531520, 1003110848, 1003689920, 1004270016, 1004850304, 1005431040, 1006012160, 1006592832, 1007174592, 1007756608, 1008339008, 1008921792, 1009504768, 1010087552, 1010671296, 1011255360, 1011839808, 1012424576, 1013009024, 1013594368, 1014180160, 1014766272, 1015352768, 1015938880, 1016526016, 1017113472, 1017701248, 1018289408, 1018877824, 1019465984, 1020055104, 1020644672, 1021234496, 1021824768, 1022414528, 1023005440, 1023596608, 1024188160, 1024780096, 1025371584, 1025964160, 1026557120, 1027150336, 1027744000, 1028337920, 1028931520, 1029526144, 1030121152, 1030716480, 1031312128, 1031907456, 1032503808, 1033100480, 1033697536, 1034294912, 1034892032, 1035490048, 1036088512, 1036687232, 1037286336, 1037885824, 1038484928, 1039085056, 1039685632, 1040286464, 1040887680, 1041488448, 1042090368, 1042692608, 1043295168, 1043898176, 1044501440, 1045104384, 1045708288, 1046312640, 1046917376, 1047522368, 1048127040, 1048732800, 1049338816, 1049945280, 1050552128, 1051158528, 1051765952, 1052373824, 1052982016, 1053590592, 1054199424,
	1054807936, 1055417600, 1056027456, 1056637760, 1057248448, 1057858752, 1058470016, 1059081728, 1059693824, 1060306304, 1060918336, 1061531392, 1062144896, 1062758656, 1063372928, 1063987392, 1064601664, 1065216896, 1065832448, 1066448448, 1067064704, 1067680704, 1068297728, 1068915136, 1069532864, 1070150976, 1070768640, 1071387520, 1072006720, 1072626240, 1073246080, 1073866368, 1074486272, 1075107200, 1075728512, 1076350208, 1076972160, 1077593856, 1078216704, 1078839680, 1079463296, 1080087040, 1080710528, 1081335168, 1081960064, 1082585344, 1083211008, 1083836928, 1084462592, 1085089280, 1085716352, 1086343936, 1086971648, 1087599104, 1088227712, 1088856576, 1089485824, 1090115456, 1090745472, 1091375104, 1092005760, 1092636928, 1093268352, 1093900160, 1094531584, 1095164160, 1095796992, 1096430336, 1097064064, 1097697280, 1098331648, 1098966400, 1099601536, 1100237056, 1100872832, 1101508224, 1102144768, 1102781824, 1103419136, 1104056832, 1104694144, 1105332608, 1105971328, 1106610432, 1107249920, 1107889152, 1108529408, 1109170048, 1109811072, 1110452352, 1111094144, 1111735552, 1112377984, 1113020928, 1113664128, 1114307712, 1114950912, 1115595264, 1116240000, 1116885120,
	1117530624, 1118175744, 1118821888, 1119468416, 1120115456, 1120762752, 1121410432, 1122057856, 1122706176, 1123355136, 1124004224, 1124653824, 1125303040, 1125953408, 1126604160, 1127255168, 1127906560, 1128557696, 1129209984, 1129862528, 1130515456, 1131168768, 1131822592, 1132475904, 1133130368, 1133785216, 1134440448, 1135096064, 1135751296, 1136407680, 1137064448, 1137721472, 1138379008, 1139036800, 1139694336, 1140353024, 1141012096, 1141671424, 1142331264, 1142990592, 1143651200, 1144312192, 1144973440, 1145635200, 1146296448, 1146958976, 1147621760, 1148285056, 1148948608, 1149612672, 1150276224, 1150940928, 1151606144, 1152271616, 1152937600, 1153603072, 1154269824, 1154936832, 1155604352, 1156272128, 1156939648, 1157608192, 1158277248, 1158946560, 1159616384, 1160286464, 1160956288, 1161627264, 1162298624, 1162970240, 1163642368, 1164314112, 1164987008, 1165660160, 1166333824, 1167007872, 1167681536, 1168356352, 1169031552, 1169707136, 1170383104, 1171059584, 1171735552, 1172412672, 1173090304, 1173768192, 1174446592, 1175124480, 1175803648, 1176483072, 1177163008, 1177843328, 1178523264, 1179204352, 1179885824, 1180567680, 1181249920, 1181932544, 1182614912, 1183298304,
	1183982208, 1184666368, 1185351040, 1186035328, 1186720640, 1187406464, 1188092672, 1188779264, 1189466368, 1190152960, 1190840832, 1191528960, 1192217600, 1192906624, 1193595136, 1194285056, 1194975232, 1195665792, 1196356736, 1197047296, 1197739136, 1198431360, 1199123968, 1199816960, 1200510336, 1201203328, 1201897600, 1202592128, 1203287040, 1203982464, 1204677504, 1205373696, 1206070272, 1206767232, 1207464704, 1208161664, 1208859904, 1209558528, 1210257536, 1210956928, 1211656832, 1212356224, 1213056768, 1213757952, 1214459392, 1215161216, 1215862656, 1216565376, 1217268352, 1217971840, 1218675712, 1219379200, 1220083840, 1220788992, 1221494528, 1222200448, 1222906752, 1223612672, 1224319872, 1225027456, 1225735424, 1226443648, 1227151616, 1227860864, 1228570496, 1229280512, 1229990912, 1230700928, 1231412096, 1232123776, 1232835840, 1233548288, 1234261248, 1234973696, 1235687424, 1236401536, 1237116032, 1237831040, 1238545536, 1239261312, 1239977472, 1240694144, 1241411072, 1242128512, 1242845568, 1243563776, 1244282496, 1245001600, 1245721088, 1246440192, 1247160448, 1247881216, 1248602368, 1249324032, 1250045184, 1250767616, 1251490432, 1252213632, 1252937344, 1253661440,
	1254385152, 1255110016, 1255835392, 1256561152, 1257287424, 1258013184, 1258740096, 1259467648, 1260195456, 1260923648, 1261651584, 1262380800, 1263110272, 1263840256, 1264570624, 1265301504, 1266031872, 1266763520, 1267495552, 1268227968, 1268961024, 1269693440, 1270427264, 1271161472, 1271896064, 1272631168, 1273365760, 1274101632, 1274838016, 1275574784, 1276311808, 1277049472, 1277786624, 1278525056, 1279264000, 1280003328, 1280743040, 1281482368, 1282222976, 1282963968, 1283705344, 1284447232, 1285188736, 1285931392, 1286674560, 1287418240, 1288162176, 1288906624, 1289650688, 1290395904, 1291141760, 1291887872, 1292634496, 1293380608, 1294128128, 1294875904, 1295624320, 1296373120, 1297122304, 1297870976, 1298621056, 1299371520, 1300122496, 1300873856, 1301624832, 1302376960, 1303129600, 1303882752, 1304636288, 1305389312, 1306143872, 1306898688, 1307654016, 1308409600, 1309165696, 1309921536, 1310678528, 1311435904, 1312193920, 1312952192, 1313710080, 1314469248, 1315228928, 1315988992, 1316749568, 1317509632, 1318271104, 1319032960, 1319795200, 1320557952, 1321321088, 1322083840, 1322847872, 1323612416, 1324377216, 1325142656, 1325907584, 1326673920, 1327440512, 1328207744,
	1328975360, 1329742464, 1330510976, 1331279872, 1332049152, 1332819072, 1333589248, 1334359168, 1335130240, 1335901824, 1336673920, 1337446400, 1338218368, 1338991744, 1339765632, 1340539904, 1341314560, 1342088832, 1342864512, 1343640576, 1344417024, 1345193984, 1345971456, 1346748416, 1347526656, 1348305408, 1349084672, 1349864320, 1350643456, 1351424000, 1352205056, 1352986496, 1353768448, 1354550784, 1355332608, 1356115968, 1356899712, 1357683840, 1358468480, 1359252608, 1360038144, 1360824192, 1361610624, 1362397440, 1363183872, 1363971712, 1364760064, 1365548672, 1366337792, 1367127424, 1367916672, 1368707200, 1369498240, 1370289664, 1371081472, 1371873024, 1372665856, 1373459072, 1374252800, 1375047040, 1375840768, 1376635904, 1377431552, 1378227584, 1379024000, 1379820928, 1380617472, 1381415296, 1382213760, 1383012480, 1383811840, 1384610560, 1385410816, 1386211456, 1387012480, 1387814144, 1388615168, 1389417728, 1390220672, 1391024128, 1391827968, 1392632320, 1393436288, 1394241536, 1395047296, 1395853568, 1396660224, 1397466368, 1398274048, 1399082112, 1399890688, 1400699648, 1401508224, 1402318080, 1403128576, 1403939456, 1404750848, 1405562624, 1406374016, 1407186816,
	1408000000, 1408813696, 1409627904, 1410441728, 1411256704, 1412072320, 1412888320, 1413704960, 1414521856, 1415338368, 1416156288, 1416974720, 1417793664, 1418612992, 1419431808, 1420252160, 1421072896, 1421894144, 1422715904, 1423537280, 1424359808, 1425183104, 1426006784, 1426830848, 1427655296, 1428479488, 1429305088, 1430131072, 1430957568, 1431784576, 1432611072, 1433438976, 1434267392, 1435096192, 1435925632, 1436754432, 1437584768, 1438415616, 1439246848, 1440078720, 1440910848, 1441742720, 1442575872, 1443409664, 1444243584, 1445078400, 1445912576, 1446748032, 1447584256, 1448420864, 1449257856, 1450094464, 1450932480, 1451771008, 1452609920, 1453449472, 1454289408, 1455128960, 1455969920, 1456811264, 1457653248, 1458495616, 1459337600, 1460180864, 1461024768, 1461869056, 1462713984, 1463558272, 1464404096, 1465250304, 1466097152, 1466944384, 1467792128, 1468639488, 1469488256, 1470337408, 1471187200, 1472037376, 1472887168, 1473738368, 1474589952, 1475442304, 1476294912, 1477148160, 1478000768, 1478854912, 1479709696, 1480564608, 1481420288, 1482275456, 1483132160, 1483989248, 1484846976, 1485704960, 1486562688, 1487421696, 1488281344, 1489141504, 1490002048, 1490863104,
	1491723776, 1492585856, 1493448448, 1494311424, 1495175040, 1496038144, 1496902656, 1497767808, 1498633344, 1499499392, 1500365056, 1501232128, 1502099712, 1502967808, 1503836416, 1504705536, 1505574016, 1506444032, 1507314688, 1508185856, 1509057408, 1509928576, 1510801280, 1511674240, 1512547840, 1513421952, 1514295680, 1515170816, 1516046464, 1516922624, 1517799296, 1518676224, 1519552896, 1520431104, 1521309824, 1522188928, 1523068800, 1523948032, 1524828672, 1525709824, 1526591616, 1527473792, 1528355456, 1529238784, 1530122496, 1531006720, 1531891712, 1532776832, 1533661824, 1534547968, 1535434880, 1536322304, 1537210112, 1538097408, 1538986368, 1539875840, 1540765696, 1541656192, 1542547072, 1543437440, 1544329472, 1545221888, 1546114944, 1547008384, 1547901440, 1548796032, 1549691136, 1550586624, 1551482752, 1552378368, 1553275520, 1554173184, 1555071232, 1555970048, 1556869248, 1557767936, 1558668288, 1559568896, 1560470272, 1561372032, 1562273408, 1563176320, 1564079616, 1564983424, 1565888000, 1566791808, 1567697408, 1568603392, 1569509760, 1570416896, 1571324416, 1572231424, 1573140096, 1574049152, 1574958976, 1575869184, 1576778752, 1577689984, 1578601728, 1579514112,
	1580426880, 1581339264, 1582253056, 1583167488, 1584082432, 1584997888, 1585913984, 1586829440, 1587746304, 1588663936, 1589582080, 1590500736, 1591418880, 1592338560, 1593258752, 1594179584, 1595100928, 1596021632, 1596944000, 1597866880, 1598790272, 1599714304, 1600638848, 1601562752, 1602488320, 1603414272, 1604340992, 1605268224, 1606194816, 1607123072, 1608051968, 1608981120, 1609911040, 1610841344, 1611771264, 1612702848, 1613634688, 1614567168, 1615500288, 1616432896, 1617367040, 1618301824, 1619237120, 1620172800, 1621108096, 1622044928, 1622982272, 1623920128, 1624858752, 1625797632, 1626736256, 1627676416, 1628616960, 1629558272, 1630499968, 1631441152, 1632384000, 1633327232, 1634271232, 1635215744, 1636159744, 1637105152, 1638051328, 1638998016, 1639945088, 1640892928, 1641840128, 1642788992, 1643738368, 1644688384, 1645638784, 1646588672, 1647540352, 1648492416, 1649445120, 1650398464, 1651351168, 1652305408, 1653260288, 1654215808, 1655171712, 1656128256, 1657084288, 1658041856, 1659000064, 1659958784, 1660918272, 1661876992, 1662837376, 1663798400, 1664759936, 1665721984, 1666683520, 1667646720, 1668610560, 1669574784, 1670539776, 1671505024, 1672470016, 1673436544,
};

#define SAMPLE_16BIT 0x01
#define SAMPLE_UNSIGNED 0x02
#define SAMPLE_LOOP 0x04
#define SAMPLE_PINGPONG 0x08
#define SAMPLE_REVERSE 0x10
#define SAMPLE_SUSTAIN 0x20
#define SAMPLE_ENVELOPE 0x40

#ifdef DEBUG_SAMPLES
#define SAMPLE_CONVERT_DEBUG(dx) printf("\r%s\n",dx)
#else
#define SAMPLE_CONVERT_DEBUG(dx)
#endif

#ifdef DEBUG_MIDI
#define MIDI_EVENT_DEBUG(dx,dy) printf("\r%s, %x\n",dx,dy)
#else
#define MIDI_EVENT_DEBUG(dx,dy)
#endif

#define WM_ERR_MEM		0
#define WM_ERR_STAT		1
#define WM_ERR_LOAD		2
#define WM_ERR_OPEN		3
#define WM_ERR_READ		4
#define WM_ERR_INVALID		5
#define WM_ERR_CORUPT		6
#define WM_ERR_NOT_INIT		7
#define WM_ERR_INVALID_ARG	8
#define WM_ERR_ALR_INIT     9

#define MAX_AUTO_AMP 2.0

#define FPBITS 10
#define FPMASK ((1L<<FPBITS)-1L)

/*
 * =========================
 * Internal Functions
 * =========================
 */


static void
WM_ERROR( const char * func, unsigned long int lne, int wmerno, const char * wmfor, int error) {
	const char * errors[] = {
		"Unable to obtain memory\0",
		"Unable to stat\0",
		"Unable to load\0",
		"Unable to open\0",
		"Unable to read\0",
		"Invalid or Unsuported file format\0",
		"File corrupt\0",
		"Library not Initialized\0",
		"Invalid argument\0"
	};
	if (wmfor != NULL) {
		if (error != 0) {
			fprintf(stderr,"\rlibWildMidi(%s:%lu): ERROR %s %s (%s)\n",func, lne, errors[wmerno], wmfor, strerror(error));
		} else {
			fprintf(stderr,"\rlibWildMidi(%s:%lu): ERROR %s %s\n",func, lne, errors[wmerno], wmfor);
		}
	} else {
		if (error != 0) {
			fprintf(stderr,"\rlibWildMidi(%s:%lu): ERROR %s (%s)\n",func, lne, errors[wmerno], strerror(error));
		} else {
			fprintf(stderr,"\rlibWildMidi(%s:%lu): ERROR %s\n",func, lne, errors[wmerno]);
		}
	}

}

static unsigned char *
WM_BufferFile (const char *filename, unsigned long int *size) {
	int buffer_fd;
	unsigned char *data;
	struct stat buffer_stat;
#ifndef _WIN32
	char *home = NULL;
	struct passwd *pwd_ent;
	char buffer_dir[1024];
#endif

	char *buffer_file = malloc(strlen(filename) + 1);

	if (buffer_file == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, errno);
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
				WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
				WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, errno);
				free(buffer_file);
				return NULL;
			}
			memmove((buffer_file + strlen(home)), (buffer_file + 1), (strlen(buffer_file)));
			strncpy (buffer_file, home,strlen(home));
		}
	} else if (buffer_file[0] != '/') {
		getcwd(buffer_dir,1024);
		if (buffer_dir[strlen(buffer_dir)-1] != '/') {
			buffer_dir[strlen(buffer_dir)+1] = '\0';
			buffer_dir[strlen(buffer_dir)] = '/';
		}
		buffer_file = realloc(buffer_file,(strlen(buffer_file) + strlen(buffer_dir) + 1));
		if (buffer_file == NULL) {
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, errno);
			free(buffer_file);
			return NULL;
		}
		memmove((buffer_file + strlen(buffer_dir)), buffer_file, strlen(buffer_file)+1);
		strncpy (buffer_file,buffer_dir,strlen(buffer_dir));
	}
#endif
	if (stat(buffer_file,&buffer_stat)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_STAT, filename, errno);
		free(buffer_file);
		return NULL;
	}
	*size = buffer_stat.st_size;
	data = malloc(*size);
	if (data == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, errno);
		free(buffer_file);
		return NULL;
	}
#ifdef _WIN32
	if ((buffer_fd = open(buffer_file,(O_RDONLY | O_BINARY))) == -1) {
#else
	if ((buffer_fd = open(buffer_file,O_RDONLY)) == -1) {
#endif
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_OPEN, filename, errno);
		free(buffer_file);
		free(data);
		return NULL;
	}
	if (read(buffer_fd,data,*size) != buffer_stat.st_size) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_READ, filename, errno);
		free(buffer_file);
		free(data);
		close(buffer_fd);
		return NULL;
	}
	close(buffer_fd);
	free(buffer_file);
	return data;
}

static inline void
WM_Lock (int * wmlock) {
	LOCK_START:
	if (__builtin_expect(((*wmlock) == 0),1)) {
		(*wmlock)++;
		if (__builtin_expect(((*wmlock) == 1), 1)) {
			return;
		}
		(*wmlock)--;
	}
#ifdef _WIN32
	Sleep(10);
#else
	usleep(500);
#endif
	goto LOCK_START;
}

static inline void
WM_Unlock (int *wmlock) {
	(*wmlock)--;
}

static void
WM_InitPatches ( void ) {
	int i;
	for (i = 0; i < 128; i++) {
		patch[i] = NULL;
	}
}

static void
WM_FreePatches ( void ) {
	int i;
	struct _patch * tmp_patch;
	struct _sample * tmp_sample;

	WM_Lock(&patch_lock);
	for (i = 0; i < 128; i++) {
		if (patch[i] != NULL) {
			while (patch[i] != NULL) {
				if (patch[i]->filename != NULL) {
					if (patch[i]->first_sample != NULL) {
						while (patch[i]->first_sample != NULL) {
							tmp_sample = patch[i]->first_sample->next;
							if (patch[i]->first_sample->data != NULL)
								free (patch[i]->first_sample->data);
							free (patch[i]->first_sample);
							patch[i]->first_sample = tmp_sample;
						}
					}
					free (patch[i]->filename);
				}
				tmp_patch = patch[i]->next;
				free(patch[i]);
				patch[i] = tmp_patch;
			}
		}
	}
	WM_Unlock(&patch_lock);
}


static char **
WM_LC_Tokenize_Line (char * line_data)
{
    int line_length = strlen(line_data);
    int line_ofs = 0;
    int token_start = 0;
    char **token_data = NULL;
    int token_count = 0;
    if (line_length != 0) {
        do {
			/*
				ignore everything after #
			*/
			if (line_data[line_ofs] == '#') {
                break;
			}

			if ((line_data[line_ofs] == ' ') || (line_data[line_ofs] == '\t')) {
				/*
					whitespace means we aren't in a token
				*/
				if (token_start) {
					token_start = 0;
					line_data[line_ofs] = '\0';
				}
			} else {
				if (!token_start) {
					/*
						the start of a token in the line
					*/
					token_start = 1;
					if ((token_data = realloc(token_data, ((token_count + 1) * sizeof(char *)))) == NULL) {
						WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse config", errno);
						return NULL;
					}

					token_data[token_count] = &line_data[line_ofs];
					token_count++;
				}
			}
			line_ofs++;
		} while (line_ofs != line_length);

		/*
			if we have found some tokens then add a null token to the end
		*/
		if (token_count) {
			token_data = realloc(token_data, ((token_count + 1) * sizeof(char *)));
			token_data[token_count] = NULL;
		}

	}

    return token_data;
}

static int
WM_LoadConfig (const char *config_file)
{
	unsigned long int config_size = 0;
	char *config_buffer =  NULL;
	char * dir_end =  NULL;
	char * config_dir =  NULL;
	unsigned long int config_ptr = 0;
	unsigned long int line_start_ptr = 0;
	unsigned short int patchid = 0;
	char * new_config = NULL;
	struct _patch * tmp_patch;
	char **line_tokens = NULL;
	int token_count = 0;


	if ((config_buffer = (char *) WM_BufferFile(config_file, &config_size)) == NULL)
    {
		WM_FreePatches();
        return -1;
	}

	dir_end = strrchr(config_file,'/');
	if (dir_end != NULL)
    {
		config_dir = malloc((dir_end - config_file + 2));
		if (config_dir == NULL)
        {
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse config", errno);
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
			WM_FreePatches();
			free (config_buffer);
			return -1;
		}
		strncpy(config_dir, config_file, (dir_end - config_file + 1));
		config_dir[dir_end - config_file + 1] = '\0';
	}
	config_ptr = 0;
	line_start_ptr = 0;
	while (config_ptr < config_size)
    {
        if (config_buffer[config_ptr] == '\r')
        {
            config_buffer[config_ptr] = ' ';
        } else if (config_buffer[config_ptr] == '\n')
        {
            config_buffer[config_ptr] = '\0';

            if (config_ptr != line_start_ptr)
            {
                if ((line_tokens = WM_LC_Tokenize_Line(&config_buffer[line_start_ptr])) != NULL) {
					if (strcasecmp(line_tokens[0],"dir") == 0) {
						if (config_dir)
						{
							free(config_dir);
						}
						config_dir = strdup(line_tokens[1]);
						if (config_dir == NULL)
						{
							WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse config", errno);
							WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
							WM_FreePatches();
							free (line_tokens);
							free (config_buffer);
							return -1;
						}
						if (config_dir[strlen(config_dir) - 1] != '/')
						{
							config_dir = realloc(config_dir,(strlen(config_dir) + 2));
							config_dir[strlen(config_dir) + 1] = '\0';
							config_dir[strlen(config_dir)] = '/';
						}
					} else if (strcasecmp(line_tokens[0],"source") == 0) {
# if (defined _WIN32) && !(defined __CYGWIN__)
						if (!((isalpha(line_tokens[1][0])) && (strncmp(&line_token[1][1]":\\",2) == 0)) && (config_dir != NULL)) {
# else
						if ((line_tokens[1][0] != '/') && (line_tokens[1][0] != '~') && (config_dir != NULL)) {
# endif
							new_config = malloc(strlen(config_dir) + strlen(line_tokens[1]) + 1);
							if (new_config == NULL)	{
								WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse config", errno);
								WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
								WM_FreePatches();
								free (config_dir);
								free (line_tokens);
								free (config_buffer);
								return -1;
							}
							strcpy(new_config,config_dir);
							strcpy(&new_config[strlen(config_dir)], line_tokens[1]);
						} else {
							new_config = malloc(strlen(line_tokens[1]) + 1);
							if (new_config == NULL) {
								WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse config", errno);
								WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
								WM_FreePatches();
								free (line_tokens);
								free (config_buffer);
								return -1;
							}
							strcpy(new_config, line_tokens[1]);
						}
						if (WM_LoadConfig(new_config) == -1)
						{
							free (new_config);
							free (line_tokens);
							free (config_buffer);
							if (config_dir != NULL)
								free (config_dir);
							return -1;
						}
						free (new_config);
					} else if (strcasecmp(line_tokens[0],"bank") == 0) {
						if (!isdigit(line_tokens[1][0])) {
							WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in bank line)", 0);
							WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
							WM_FreePatches();
							if (config_dir != NULL)
								free (config_dir);
							free (line_tokens);
							free (config_buffer);
							return -1;
						}
						patchid = (atoi(line_tokens[1]) & 0xFF ) << 8;
					} else if (strcasecmp(line_tokens[0],"drumset") == 0) {
						if (!isdigit(line_tokens[1][0])) {
							WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in drumset line)", 0);
							WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
							WM_FreePatches();
							if (config_dir != NULL)
								free (config_dir);
							free (line_tokens);
							free (config_buffer);
							return -1;
						}
						patchid = ((atoi(line_tokens[1]) & 0xFF ) << 8) | 0x80;
					} else if (isdigit(line_tokens[0][0])) {
						patchid = (patchid & 0xFF80) | (atoi(line_tokens[0]) & 0x7F);
						if (patch[(patchid & 0x7F)] == NULL) {
							patch[(patchid & 0x7F)] = malloc (sizeof(struct _patch));
							if (patch[(patchid & 0x7F)] == NULL) {
								WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
								WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
								WM_FreePatches();
								if (config_dir != NULL)
									free (config_dir);
								free (line_tokens);
								free (config_buffer);
								return -1;
							}
							tmp_patch = patch[(patchid & 0x7F)];
							tmp_patch->patchid = patchid;
							tmp_patch->filename = NULL;
							tmp_patch->amp = 1024;
							tmp_patch->note = 0;
							tmp_patch->next = NULL;
							tmp_patch->first_sample = NULL;
							tmp_patch->loaded = 0;
							tmp_patch->inuse_count = 0;
						} else {
							tmp_patch = patch[(patchid & 0x7F)];
							if (tmp_patch->patchid == patchid)
							{
								free (tmp_patch->filename);
								tmp_patch->filename = NULL;
								tmp_patch->amp = 1024;
								tmp_patch->note = 0;
							} else {
								if (tmp_patch->next != NULL)
								{
									while (tmp_patch->next != NULL)
									{
										if (tmp_patch->next->patchid == patchid)
											break;
										tmp_patch = tmp_patch->next;
									}
									if (tmp_patch->next == NULL)
									{
										if ((tmp_patch->next = malloc (sizeof(struct _patch))) == NULL)
										{
											WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, 0);
											WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
											WM_FreePatches();
											if (config_dir != NULL)
												free (config_dir);
											free (line_tokens);
											free (config_buffer);
											return -1;
										}
										tmp_patch = tmp_patch->next;
										tmp_patch->patchid = patchid;
										tmp_patch->filename = NULL;
										tmp_patch->amp = 1024;
										tmp_patch->note = 0;
										tmp_patch->next = NULL;
										tmp_patch->first_sample = NULL;
										tmp_patch->loaded = 0;
										tmp_patch->inuse_count = 0;
									} else {
										tmp_patch = tmp_patch->next;
										free (tmp_patch->filename);
										tmp_patch->filename = NULL;
										tmp_patch->amp = 1024;
										tmp_patch->note = 0;
									}
								} else {
									tmp_patch->next = malloc (sizeof(struct _patch));
									if (tmp_patch->next == NULL) {
										WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
										WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
										WM_FreePatches();
										if (config_dir != NULL)
											free (config_dir);
										free (line_tokens);
										free (config_buffer);
										return -1;
									}
									tmp_patch = tmp_patch->next;
									tmp_patch->patchid = patchid;
									tmp_patch->filename = NULL;
									tmp_patch->amp = 1024;
									tmp_patch->note = 0;
									tmp_patch->next = NULL;
									tmp_patch->first_sample = NULL;
									tmp_patch->loaded = 0;
									tmp_patch->inuse_count = 0;
								}
							}
				        }
						if (config_dir != NULL)
						{
							tmp_patch->filename = malloc(strlen(config_dir) + strlen(line_tokens[1]) + 1);
							if (tmp_patch->filename == NULL)
							{
								WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, 0);
								WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
								WM_FreePatches();
								free (config_dir);
								free (line_tokens);
								free (config_buffer);
								return -1;
							}
							strcpy(tmp_patch->filename, config_dir);
							strcat(tmp_patch->filename, line_tokens[1]);
						} else
						{
							tmp_patch->filename = malloc(strlen(line_tokens[1]) + 1);
							if (tmp_patch->filename == NULL)
							{
								WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, 0);
								WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
								WM_FreePatches();
								if (config_dir)
									free (config_dir);
								free (line_tokens);
								free (config_buffer);
								return -1;
							}
							strcpy(tmp_patch->filename, line_tokens[1]);
						}
						if (strncasecmp(&tmp_patch->filename[strlen(tmp_patch->filename) - 4], ".pat", 4) != 0)
						{
							tmp_patch->filename = realloc(tmp_patch->filename, strlen(tmp_patch->filename) + 5);
							if (tmp_patch->filename == NULL)
							{
								WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, 0);
								WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
								WM_FreePatches();
								if (config_dir)
									free (config_dir);
								free (line_tokens);
								free (config_buffer);
								return -1;
							}
							strcat(tmp_patch->filename, ".pat");
						}
						tmp_patch->env[0].set = 0x00;
						tmp_patch->env[1].set = 0x00;
						tmp_patch->env[2].set = 0x00;
						tmp_patch->env[3].set = 0x00;
						tmp_patch->env[4].set = 0x00;
						tmp_patch->env[5].set = 0x00;
						tmp_patch->keep = 0;
						tmp_patch->remove = 0;

						token_count = 0;
						while (line_tokens[token_count] != NULL)
						{
							if (strncasecmp(line_tokens[token_count], "amp=", 4) == 0)
							{
								if (!isdigit(line_tokens[token_count][4]))
								{
									WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch 	line)", 0);
								} else
								{
									tmp_patch->amp = (atoi(&line_tokens[token_count][4]) << 10) / 100;
								}
							} else if (strncasecmp(line_tokens[token_count], "note=", 5) == 0)
							{
								if (!isdigit(line_tokens[token_count][5]))
								{
									WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch 	line)", 0);
								} else
								{
									tmp_patch->note = (atoi(&line_tokens[token_count][5]) << 10) / 100;
								}
							} else if (strncasecmp(line_tokens[token_count], "env_time", 8) == 0)
							{
								if ((!isdigit(line_tokens[token_count][8])) || (!isdigit(line_tokens[token_count][10])) || (line_tokens[token_count][9] != '='))
								{
									WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch 	line)", 0);
								} else {
									unsigned int env_no = atoi(&line_tokens[token_count][8]);
									if (env_no > 5) {
										WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch 	line)", 0);
									} else {
										tmp_patch->env[env_no].time = atof(&line_tokens[token_count][10]);
										if ((tmp_patch->env[env_no].time > 45000.0) || (tmp_patch->env[env_no].time < 1.47))
										{
											WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
											tmp_patch->env[env_no].set &= 0xFE;
										} else
										{
											tmp_patch->env[env_no].set |= 0x01;
										}
									}
								}
							} else if (strncasecmp(line_tokens[token_count], "env_level", 9) == 0)
							{
								if ((!isdigit(line_tokens[token_count][9])) || (!isdigit(line_tokens[token_count][11])) || (line_tokens[token_count][10] != '='))
								{
									WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch 	line)", 0);
								} else {
									unsigned int env_no = atoi(&line_tokens[token_count][9]);
									if (env_no > 5) {
										WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
									} else {
										tmp_patch->env[env_no].level = atof(&line_tokens[token_count][11]);
										if ((tmp_patch->env[env_no].level > 1.0) || (tmp_patch->env[env_no].level < 0.0)) {
											WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
											tmp_patch->env[env_no].set &= 0xFD;
										} else {
											tmp_patch->env[env_no].set |= 0x02;
										}
									}
								}
							} else if (strcasecmp(line_tokens[token_count], "keep=loop") == 0)
							{
								tmp_patch->keep |= SAMPLE_LOOP;
							} else if (strcasecmp(line_tokens[token_count], "keep=env") == 0)
							{
								tmp_patch->keep |= SAMPLE_ENVELOPE;
							} else if (strcasecmp(line_tokens[token_count], "remove=sustain") == 0)
							{
								tmp_patch->remove |= SAMPLE_SUSTAIN;
							}
							token_count++;
						}
					}
				}
                /*
					free up tokens
				*/
                free(line_tokens);
            }
            line_start_ptr = config_ptr + 1;
        }
        config_ptr++;
    }
	free (config_buffer);

	if (config_dir != NULL)
		free (config_dir);

	return 0;
}


#if 0

/*
	convert_8to16 - convert an 8bit sample to 16bits

	signed short int *convert_8to16 (unsigned char *data, unsigned long int size)

	data		address of where the 8bit sample is stored
	size		the size in bytes of the sample

	Returns
		The address where the 16bit sample is stored
		NULL		Error

	NOTE: Calling function will need to free the data once no-longer needed
*/

static inline signed short int *
convert_8to16 (unsigned char *data, unsigned long int size) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + size;
	signed short int *new_data = NULL;
	signed short int *write_data = NULL;

	/*
		Grab enough ram for new sample
	*/
	if ((new_data = calloc(size, sizeof(signed short int))) == NULL) {
		/*
			An error occured when trying to grab some ram
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to convert sample", errno);
		return NULL;
	}

	write_data = new_data;

	/*
		Turn the 8bit sample into a 16bit one
	*/
	do {
		*write_data = (*read_data++) << 8;
		write_data++;
	} while (read_data != read_end);

	/*
		Return the memory location where the new sample is stored
	*/
	return new_data;
}

/*
	convert_sign - convert sample from unsigned to signed (or visa-versa)

	void convert_sign (signed short int *data, unsigned long int size)

	data		address of where the 16bit sample is stored
	size		the size in bytes of the sample

	Returns Nothing
*/

static inline void
convert_sign (signed short int *data, unsigned long int size) {
	signed short int *read_data = data;
	signed short int *read_end = data + size;

	/*
		convert from unsigned sample to signed (or visa-versa)
	*/
	do {
		*read_data ^= 0x8000;
		read_data++;
	} while (read_data < read_end);

	/*
		Done
	*/
	return;
}

/*
	convert_reverse - reverse the sample

	signed short int *convert_reverse (signed short int *data, unsigned long int size)

	data		address of where the 16bit sample is stored
	size		the size in bytes of the sample

	Returns Nothing
		The address where the 16bit sample is stored
		NULL		Error

	NOTE: Calling function will need to free the data once no-longer needed

	===============================================================

	sample end ------------loop end---------loop start--------sample start

	===============================================================
*/

static inline signed short int *
convert_reverse (signed short int *data, unsigned long int size) {
	signed short int *read_data = data;
	signed short int *read_end = data + size;
	signed short int *write_data = NULL;
	signed short int *new_data = NULL;

	/*
		Grab enough ram for new sample
	*/
	new_data = calloc(size, sizeof(signed short int));
	if (new_data == NULL) {
		/*
			An error occured when trying to grab some ram
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to convert sample", errno);
		return NULL;
	}


	write_data = new_data + size;

	/*
		Read the samples in one direction, write them in the other
	*/
	do {
		write_data--;
		*write_data = *read_data++;
	} while (read_data != read_end);

	/*
		Free up data since it is not needed any more
	*/
	free(data);

	/*
		Return the memory location where the new sample is stored
	*/
	return new_data;
}

/*
	convert_pingpong - make the sample able to play straight through

	signed short int *convert_pingpong (signed short int *data, unsigned long int size, unsigned long int loop_start, unsigned long int loop_end)

	data		address of where the 16bit sample is stored
	size		the size in bytes of the sample
	loop_start	the start of the sample loop in bytes
	loop_end	the end of the sample loop in bytes

	Returns Nothing
		The address where the 16bit sample is stored
		NULL		Error

	NOTE: Calling function will need to free the data once no-longer needed

	===============================================================

	sample start ------------loop start---------loop end--------sample end

	when playing a sample with a pingpong mode, you play in the following way

	sample start -> loop end
	loop end -> loop start
	loop start -> loop end

	bouncing back and forth between loop start and loop end.

	When you want to end the note, if you are currently sampling from
	loop end to loop start then you continue until you reach loop start then

	loop start -> sample end

	===============================================================
*/

static inline signed short int *
convert_pingpong (signed short int *data, unsigned long int size, unsigned long int loop_start, unsigned long int loop_end) {

	signed short int *read_data = NULL;
	signed short int *read_end = NULL;
	signed short int *write_data = NULL;

	signed short int *new_data = NULL;
	unsigned long int dloop_size = ((loop_end - loop_start) << 1);
	unsigned long int new_size = size + dloop_size;

	/*
		Grab enough ram for new sample
	*/
	new_data = calloc(new_size, sizeof(signed short int));
	if (new_data == NULL) {
		/*
			An error occured when trying to grab some ram
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to convert sample", errno);
		return NULL;
	}

	/*
		Store from sample start to loop end
	*/
	read_data = data;
	read_end = data + loop_end;
	write_data = new_data;
	do {
		*write_data = *read_data++;
		write_data++;
	} while (read_data != read_end);

	/*
		Store from loop end to loop start
	*/
	read_end = data + loop_start;
	do {
		*write_data = *read_data--;
		write_data++;
	} while (read_data != read_end);


	if (loop_end != size) {
		/*
			Store from loop start to sample end
		*/
		read_data = data + loop_start;
		read_end = data + size;
		do {
			*write_data = *read_data++;
			write_data++;
		} while (read_data != read_end);
	}

	/*
		Free up data since it is not needed any more
	*/
	free(data);

	/*
		Return the memory location where the new sample is stored
	*/
	return new_data;
}

/* sample loading */

static int
load_sample (struct _patch *sample_patch) {
	unsigned char *gus_patch;
	unsigned long int gus_size;
	unsigned long int gus_ptr;
	unsigned char no_of_samples;
	struct _sample *gus_sample = NULL;
	unsigned long int i = 0;
	unsigned long int tmp_loop;


	//SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	//SAMPLE_CONVERT_DEBUG(sample_patch->filename);
	sample_patch->loaded = 1;
	if ((gus_patch = WM_BufferFile(sample_patch->filename,&gus_size)) == NULL) {
		return -1;
	}
	if (gus_size < 239) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, sample_patch->filename, 0);
		free(gus_patch);
		return -1;
	}
	if (memcmp(gus_patch, "GF1PATCH110\0ID#000002", 22) && memcmp(gus_patch, "GF1PATCH100\0ID#000002", 22)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID,"(unsupported format)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, sample_patch->filename, 0);
		free(gus_patch);
		return -1;
	}
	if (gus_patch[82] > 1) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID,"(unsupported format)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, sample_patch->filename, 0);
		free(gus_patch);
		return -1;
	}
	if (gus_patch[151] > 1) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID,"(unsupported format)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, sample_patch->filename, 0);
		free(gus_patch);
		return -1;
	}

	no_of_samples = gus_patch[198];
	sample_patch->first_sample = NULL;
	gus_ptr = 239;
	while (no_of_samples) {
		unsigned long int tmp_cnt;
		if (sample_patch->first_sample == NULL) {
			sample_patch->first_sample = malloc(sizeof(struct _sample));
			gus_sample = sample_patch->first_sample;
		} else {
			gus_sample->next = malloc(sizeof(struct _sample));
			gus_sample = gus_sample->next;
		}
		if (gus_sample == NULL) {
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, 0);
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, sample_patch->filename, 0);
			return -1;
		}

		gus_sample->next = NULL;
		gus_sample->loop_fraction = gus_patch[gus_ptr+7];
		gus_sample->data_length = (gus_patch[gus_ptr+11] << 24) | (gus_patch[gus_ptr+10] << 16) | (gus_patch[gus_ptr+9] << 8) | gus_patch[gus_ptr+8];
		gus_sample->loop_start = (gus_patch[gus_ptr+15] << 24) | (gus_patch[gus_ptr+14] << 16) | (gus_patch[gus_ptr+13] << 8) | gus_patch[gus_ptr+12];
		gus_sample->loop_end = (gus_patch[gus_ptr+19] << 24) | (gus_patch[gus_ptr+18] << 16) | (gus_patch[gus_ptr+17] << 8) | gus_patch[gus_ptr+16];
		gus_sample->rate = (gus_patch[gus_ptr+21] << 8) | gus_patch[gus_ptr+20];
		gus_sample->freq_low = ((gus_patch[gus_ptr+25] << 24) | (gus_patch[gus_ptr+24] << 16) | (gus_patch[gus_ptr+23] << 8) | gus_patch[gus_ptr+22]);
		gus_sample->freq_high = ((gus_patch[gus_ptr+29] << 24) | (gus_patch[gus_ptr+28] << 16) | (gus_patch[gus_ptr+27] << 8) | gus_patch[gus_ptr+26]);
		gus_sample->freq_root = ((gus_patch[gus_ptr+33] << 24) | (gus_patch[gus_ptr+32] << 16) | (gus_patch[gus_ptr+31] << 8) | gus_patch[gus_ptr+30]);

		/* This is done this way instead of ((freq * 1024) / rate) to avoid 32bit overflow. */
		/* Result is 0.001% inacurate */
		gus_sample->inc_div = ((gus_sample->freq_root * 512) / gus_sample->rate) * 2;


//		printf("\rTremolo Sweep: %i, Rate: %i, Depth %i\n",
//			gus_patch[gus_ptr+49], gus_patch[gus_ptr+50], gus_patch[gus_ptr+51]);
//		printf("\rVibrato Sweep: %i, Rate: %i, Depth %i\n",
//			gus_patch[gus_ptr+52], gus_patch[gus_ptr+53], gus_patch[gus_ptr+54]);

		gus_sample->modes = gus_patch[gus_ptr+55] & 0x7F;
		if ((sample_patch->remove & SAMPLE_SUSTAIN) && (gus_sample->modes & SAMPLE_SUSTAIN)) {
			gus_sample->modes ^= SAMPLE_SUSTAIN;
		}
		if (sample_patch->patchid & 0x0080) {
			if (!(sample_patch->keep & SAMPLE_LOOP)) {
 				gus_sample->modes &= 0xFB;
			}
			if (!(sample_patch->keep & SAMPLE_ENVELOPE)) {
				gus_sample->modes &= 0xBF;
			}
		}


		if (gus_sample->loop_start > gus_sample->loop_end) {
			tmp_loop = gus_sample->loop_end;
			gus_sample->loop_end = gus_sample->loop_start;
			gus_sample->loop_start = tmp_loop;
			gus_sample->loop_fraction  = ((gus_sample->loop_fraction & 0x0f) << 4) | ((gus_sample->loop_fraction & 0xf0) >> 4);
		}
		for (i = 0; i < 6; i++) {
			if (gus_sample->modes & SAMPLE_ENVELOPE) {
				unsigned char env_rate = gus_patch[gus_ptr+37+i];
				if (sample_patch->env[i].set & 0x02) {
					gus_sample->env_target[i] = 16448 * (unsigned long int)(255.0 * sample_patch->env[i].level);
				} else {
					gus_sample->env_target[i] = 16448 * gus_patch[gus_ptr+43+i];
				}

				if (sample_patch->env[i].set & 0x01) {
					gus_sample->env_rate[i]  = (unsigned long int)(4194303.0 / ((float)WM_SampleRate * (sample_patch->env[i].time / 1000.0)));
				} else {
					gus_sample->env_rate[i]  = (unsigned long int)(4194303.0 / ((float)WM_SampleRate * env_time_table[env_rate]));
					if (gus_sample->env_rate[i] == 0) {
						fprintf(stderr,"\rWarning: libWildMidi %s found invalid envelope(%lu) rate setting in %s. Using %f instead.\n",__FUNCTION__, i, sample_patch->filename, env_time_table[63]);
						gus_sample->env_rate[i]  = (unsigned long int)(4194303.0 / ((float)WM_SampleRate * env_time_table[63]));
					}
				}
			} else {
				gus_sample->env_target[i] = 4194303;
				gus_sample->env_rate[i]  = (unsigned long int)(4194303.0 / ((float)WM_SampleRate * env_time_table[63]));
			}
		}

		gus_sample->env_target[6] = 0;
		gus_sample->env_rate[6]  = (unsigned long int)(4194303.0 / ((float)WM_SampleRate * env_time_table[63]));

		if ((sample_patch->patchid == 47) && (!(gus_sample->modes & SAMPLE_LOOP))) {
			for (i = 3; i < 6; i++) {
				gus_sample->env_target[i] = gus_sample->env_target[2];
				gus_sample->env_rate[i] = gus_sample->env_rate[2];
			}
		}

		gus_ptr += 96;
		tmp_cnt = gus_sample->data_length;

/* convert to float */
		gus_sample->min_peek = 0;
		gus_sample->max_peek = 0;

		/*
			copy the sample
		*/
		if ((gus_sample->data = malloc(gus_sample->data_length)) == NULL) {
			/*
				Error getting memory
			*/
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, 0);
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, sample_patch->filename, 0);
			free(gus_patch);
			return -1;
		}

		memcpy(gus_sample->data, &gus_patch[gus_ptr], gus_sample->data_length);

		/*
			if 8bit, convert to 16bit
		*/
		if ((gus_sample->modes & SAMPLE_16BIT)) {
			gus_sample->data_length >>= 1;
			gus_sample->loop_start >>= 1;
			gus_sample->loop_end >>= 1;
		} else {
			if ((gus_sample->data = convert_8to16((unsigned char *)gus_sample->data, gus_sample->data_length)) == NULL) {
				free(gus_patch);
				return -1;
			}
		}

		/*
			if unsigned, convert to signed
		*/
		if ((gus_sample->modes & SAMPLE_UNSIGNED)) {
			convert_sign(gus_sample->data, gus_sample->data_length);
		}

		/*
			switch around if reversed
		*/
		if ((gus_sample->modes & SAMPLE_REVERSE)) {
			unsigned long int temp_loop_start = 0;
			if ((gus_sample->data = convert_reverse(gus_sample->data, gus_sample->data_length)) == NULL) {
				free(gus_patch);
				return -1;
			}

			temp_loop_start = gus_sample->loop_start;
			gus_sample->loop_start = gus_sample->loop_end;
			gus_sample->loop_end = temp_loop_start;
			gus_sample->loop_fraction = ((gus_sample->loop_fraction & 0x0f) << 4) | ((gus_sample->loop_fraction & 0xf0) >> 4);
		}

		/*
			if pingpong then straighten it out a bit
		*/
		if ((gus_sample->modes & SAMPLE_PINGPONG)) {
			unsigned long int tmp_loop_size = 0;
			if ((gus_sample->data = convert_pingpong(gus_sample->data, gus_sample->data_length, gus_sample->loop_start, gus_sample->loop_end)) == NULL) {
				free(gus_patch);;
				return -1;
			}
			tmp_loop_size = (gus_sample->loop_end - gus_sample->loop_start) << 1;
			gus_sample->loop_end = gus_sample->loop_start + (tmp_loop_size);
			gus_sample->data_length += tmp_loop_size;
		}

// FIXME:
		gus_sample->max_peek = 32767;
		gus_sample->min_peek = -32767;

// =======

		if (gus_sample->max_peek > (-gus_sample->min_peek)) {
			gus_sample->peek_adjust = 33553408 / gus_sample->max_peek;
		} else {
			gus_sample->peek_adjust = 33554432 / (-gus_sample->min_peek);
		}

		gus_sample->peek_adjust = (gus_sample->peek_adjust * sample_patch->amp) >> 10;

		gus_ptr += tmp_cnt;
		gus_sample->loop_start = (gus_sample->loop_start << 10) | (((gus_sample->loop_fraction & 0x0f) << 10) / 16);
		gus_sample->loop_end = (gus_sample->loop_end << 10) | (((gus_sample->loop_fraction & 0xf0) << 6) / 16);
		gus_sample->loop_size = gus_sample->loop_end - gus_sample->loop_start;
		gus_sample->data_length = gus_sample->data_length << 10;
		no_of_samples--;
	}
	free(gus_patch);
	return 0;
}

#else

/*
 * sample data conversion functions
 * convert data to signed shorts
 */

/* 8bit signed */
int
convert_8s (unsigned char *data, struct _sample *gus_sample ) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc((gus_sample->data_length + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data;
		do {
			*write_data = (*read_data++) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data != read_end);
		return 0;
	}

	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 8bit signed ping pong */
int
convert_8sp (unsigned char *data, struct _sample *gus_sample ) {
	unsigned long int loop_length = gus_sample->loop_end - gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->loop_start;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc((new_length + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data;
		do {
			*write_data = (*read_data++) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data != read_end);

		*write_data = (*read_data++ << 8);
		write_data_a = write_data + dloop_length;
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + dloop_length;
		read_end = data + gus_sample->loop_end;
		do {
			*write_data = (*read_data++) << 8;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data != read_end);

		*write_data = (*read_data++ << 8);
		*write_data_b++ = *write_data;
		read_end = data + gus_sample->data_length;
		if (__builtin_expect((read_data != read_end),1)) {
			do {
				*write_data_b = (*read_data++) << 8;
				if (*write_data_b > gus_sample->max_peek) {
					gus_sample->max_peek = *write_data_b;
				} else if (*write_data_b < gus_sample->min_peek) {
					gus_sample->min_peek = *write_data_b;
				}
				write_data_b++;
			} while (read_data != read_end);
		}
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG;
		return 0;
	}

	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 8bit signed reverse */
int
convert_8sr (unsigned char *data, struct _sample *gus_sample ) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;
	unsigned long int tmp_loop = 0;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc((gus_sample->data_length + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data + gus_sample->data_length - 1;
		do {
			*write_data = (*read_data++) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data--;
		} while (read_data != read_end);
		tmp_loop = gus_sample->loop_end;
		gus_sample->loop_end = gus_sample->data_length - gus_sample->loop_start;
		gus_sample->loop_start = gus_sample->data_length - tmp_loop;
		gus_sample->loop_fraction  = ((gus_sample->loop_fraction & 0x0f) << 4) | ((gus_sample->loop_fraction & 0xf0) >> 4);
		gus_sample->modes ^= SAMPLE_REVERSE;
		return 0;
	}
	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}


/* 8bit signed reverse ping pong */
int
convert_8srp (unsigned char *data, struct _sample *gus_sample ) {
	unsigned long int loop_length = gus_sample->loop_end - gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data + gus_sample->data_length - 1;
	unsigned char *read_end = data + gus_sample->loop_end;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc((new_length + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data;
		do {
			*write_data = (*read_data--) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data != read_end);

		*write_data = (*read_data-- << 8);
		write_data_a = write_data + dloop_length;
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + dloop_length;
		read_end = data + gus_sample->loop_start;
		do {
			*write_data = (*read_data--) << 8;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data != read_end);

		*write_data = (*read_data-- << 8);
		*write_data_b++ = *write_data;
		read_end = data - 1;
		do {
			*write_data_b = (*read_data--) << 8;
			if (*write_data_b > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data_b;
			} else if (*write_data_b < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data_b;
			}
			write_data_b++;
		} while (read_data != read_end);
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_REVERSE;
		return 0;
	}

	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 8bit unsigned */
int
convert_8u (unsigned char *data, struct _sample *gus_sample ) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc((gus_sample->data_length + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data;
		do {
			*write_data = ((*read_data++) ^ 0x80) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data != read_end);
		gus_sample->modes ^= SAMPLE_UNSIGNED;
		return 0;
	}
	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 8bit unsigned ping pong */
int
convert_8up (unsigned char *data, struct _sample *gus_sample ) {
	unsigned long int loop_length = gus_sample->loop_end - gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->loop_start;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc((new_length + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data;
		do {
			*write_data = ((*read_data++) ^ 0x80) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data != read_end);

		*write_data = ((*read_data++) ^ 0x80) << 8;
		write_data_a = write_data + dloop_length;
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + dloop_length;
		read_end = data + gus_sample->loop_end;
		do {
			*write_data = ((*read_data++) ^ 0x80) << 8;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data != read_end);

		*write_data = ((*read_data++) ^ 0x80) << 8;
		*write_data_b++ = *write_data;
		read_end = data + gus_sample->data_length;
		if (__builtin_expect((read_data != read_end),1)) {
			do {
				*write_data_b = ((*read_data++) ^ 0x80) << 8;
				if (*write_data_b > gus_sample->max_peek) {
					gus_sample->max_peek = *write_data_b;
				} else if (*write_data_b < gus_sample->min_peek) {
					gus_sample->min_peek = *write_data_b;
				}
				write_data_b++;
			} while (read_data != read_end);
		}
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_UNSIGNED;
		return 0;
	}

	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 8bit unsigned reverse */
int
convert_8ur (unsigned char *data, struct _sample *gus_sample ) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;
	unsigned long int tmp_loop = 0;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc((gus_sample->data_length + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data + gus_sample->data_length - 1;
		do {
			*write_data = ((*read_data++) ^ 0x80) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data--;
		} while (read_data != read_end);
		tmp_loop = gus_sample->loop_end;
		gus_sample->loop_end = gus_sample->data_length - gus_sample->loop_start;
		gus_sample->loop_start = gus_sample->data_length - tmp_loop;
		gus_sample->loop_fraction  = ((gus_sample->loop_fraction & 0x0f) << 4) | ((gus_sample->loop_fraction & 0xf0) >> 4);
		gus_sample->modes ^= SAMPLE_REVERSE | SAMPLE_UNSIGNED;
		return 0;
	}
	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 8bit unsigned reverse ping pong */
int
convert_8urp (unsigned char *data, struct _sample *gus_sample ) {
	unsigned long int loop_length = gus_sample->loop_end - gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data + gus_sample->data_length - 1;
	unsigned char *read_end = data + gus_sample->loop_end;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc((new_length + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data;
		do {
			*write_data = ((*read_data--) ^ 0x80) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data != read_end);

		*write_data = ((*read_data--) ^ 0x80) << 8;
		write_data_a = write_data + dloop_length;
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + dloop_length;
		read_end = data + gus_sample->loop_start;
		do {
			*write_data = ((*read_data--) ^ 0x80) << 8;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data != read_end);

		*write_data = ((*read_data--) ^ 0x80) << 8;
		*write_data_b++ = *write_data;
		read_end = data - 1;
		do {
			*write_data_b = ((*read_data--) ^ 0x80) << 8;
			if (*write_data_b > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data_b;
			} else if (*write_data_b < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data_b;
			}
			write_data_b++;
		} while (read_data != read_end);
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_REVERSE | SAMPLE_UNSIGNED;
		return 0;
	}

	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit signed */
int
convert_16s (unsigned char *data, struct _sample *gus_sample ) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc(((gus_sample->data_length >> 1) + 1),sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data;
		do {
			*write_data = *read_data++;
			*write_data |= (*read_data++) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data < read_end);

		gus_sample->loop_start >>= 1;
		gus_sample->loop_end >>= 1;
		gus_sample->data_length >>= 1;
		return 0;
	}
	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit signed ping pong */
int
convert_16sp (unsigned char *data, struct _sample *gus_sample ) {
	unsigned long int loop_length = gus_sample->loop_end - gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->loop_start;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc(((new_length >> 1) + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data;
		do {
			*write_data = (*read_data++);
			*write_data |= (*read_data++) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data < read_end);

		*write_data = (*read_data++);
		*write_data |= (*read_data++) << 8;
		write_data_a = write_data + (dloop_length >> 1);
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + (dloop_length >> 1);
		read_end = data + gus_sample->loop_end;
		do {
			*write_data = (*read_data++);
			*write_data |= (*read_data++) << 8;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data < read_end);

		*write_data = *(read_data++);
		*write_data |= (*read_data++) << 8;
		*write_data_b++ = *write_data;
		read_end = data + gus_sample->data_length;
		if (__builtin_expect((read_data != read_end),1)) {
			do {
				*write_data_b = *(read_data++);
				*write_data_b |= (*read_data++) << 8;
				if (*write_data_b > gus_sample->max_peek) {
					gus_sample->max_peek = *write_data_b;
				} else if (*write_data_b < gus_sample->min_peek) {
					gus_sample->min_peek = *write_data_b;
				}
				write_data_b++;
			} while (read_data < read_end);
		}
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG;
		gus_sample->loop_start >>= 1;
		gus_sample->loop_end >>= 1;
		gus_sample->data_length >>= 1;
		return 0;
	}

	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit signed reverse */
int
convert_16sr (unsigned char *data, struct _sample *gus_sample ) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;
	unsigned long int tmp_loop = 0;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc(((gus_sample->data_length >> 1) + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data + (gus_sample->data_length >> 1) - 1;
		do {
			*write_data = *read_data++;
			*write_data |= (*read_data++) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data--;
		} while (read_data < read_end);
		tmp_loop = gus_sample->loop_end;
		gus_sample->loop_end = gus_sample->data_length - gus_sample->loop_start;
		gus_sample->loop_start = gus_sample->data_length - tmp_loop;
		gus_sample->loop_fraction  = ((gus_sample->loop_fraction & 0x0f) << 4) | ((gus_sample->loop_fraction & 0xf0) >> 4);
		gus_sample->loop_start >>= 1;
		gus_sample->loop_end >>= 1;
		gus_sample->data_length >>= 1;
		gus_sample->modes ^= SAMPLE_REVERSE;
		return 0;
	}
	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}


/* 16bit signed reverse ping pong */
int
convert_16srp (unsigned char *data, struct _sample *gus_sample ) {
	unsigned long int loop_length = gus_sample->loop_end - gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data + gus_sample->data_length - 1;
	unsigned char *read_end = data + gus_sample->loop_end;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc(((new_length >> 1) + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data;
		do {

			*write_data = (*read_data--) << 8;
			*write_data |= *read_data--;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data < read_end);

		*write_data = (*read_data-- << 8);
		*write_data |= *read_data--;
		write_data_a = write_data + (dloop_length >> 1);
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + (dloop_length >> 1);
		read_end = data + gus_sample->loop_start;
		do {
			*write_data = (*read_data--) << 8;
			*write_data |= *read_data--;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data < read_end);

		*write_data = ((*read_data--) << 8);
		*write_data |= *read_data--;
		*write_data_b++ = *write_data;
		read_end = data - 1;
		do {
			*write_data_b = (*read_data--) << 8;
			*write_data_b |= *read_data--;
			if (*write_data_b > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data_b;
			} else if (*write_data_b < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data_b;
			}
			write_data_b++;
		} while (read_data < read_end);
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_REVERSE;
		return 0;
	}

	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit unsigned */
int
convert_16u (unsigned char *data, struct _sample *gus_sample ) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc(((gus_sample->data_length >> 1) + 1),sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data;
		do {
			*write_data = *read_data++;
			*write_data |= ((*read_data++) ^ 0x80) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data < read_end);
		gus_sample->loop_start >>= 1;
		gus_sample->loop_end >>= 1;
		gus_sample->data_length >>= 1;
		gus_sample->modes ^= SAMPLE_UNSIGNED;
		return 0;
	}
	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit unsigned ping pong */
int
convert_16up (unsigned char *data, struct _sample *gus_sample ) {
	unsigned long int loop_length = gus_sample->loop_end - gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->loop_start;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc(((new_length >> 1) + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data;
		do {
			*write_data = (*read_data++);
			*write_data |= ((*read_data++) ^ 0x80) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data < read_end);

		*write_data = (*read_data++);
		*write_data |= ((*read_data++) ^ 0x80) << 8;
		write_data_a = write_data + (dloop_length >> 1);
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + (dloop_length >> 1);
		read_end = data + gus_sample->loop_end;
		do {
			*write_data = (*read_data++);
			*write_data |= ((*read_data++) ^ 0x80) << 8;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data < read_end);

		*write_data = (*read_data++);
		*write_data |= ((*read_data++) ^ 0x80) << 8;
		*write_data_b++ = *write_data;
		read_end = data + gus_sample->data_length;
		if (__builtin_expect((read_data != read_end),1)) {
			do {
				*write_data_b = (*read_data++);
				*write_data_b |= ((*read_data++) ^ 0x80) << 8;
				if (*write_data_b > gus_sample->max_peek) {
					gus_sample->max_peek = *write_data_b;
				} else if (*write_data_b < gus_sample->min_peek) {
					gus_sample->min_peek = *write_data_b;
				}
				write_data_b++;
			} while (read_data < read_end);
		}
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG;
		gus_sample->loop_start >>= 1;
		gus_sample->loop_end >>= 1;
		gus_sample->data_length >>= 1;
		return 0;
	}

	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit unsigned reverse */
int
convert_16ur (unsigned char *data, struct _sample *gus_sample ) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;
	unsigned long int tmp_loop = 0;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc(((gus_sample->data_length >> 1) + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data + (gus_sample->data_length >> 1) - 1;
		do {
			*write_data = *read_data++;
			*write_data |= ((*read_data++) ^ 0x80) << 8;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data--;
		} while (read_data < read_end);
		tmp_loop = gus_sample->loop_end;
		gus_sample->loop_end = gus_sample->data_length - gus_sample->loop_start;
		gus_sample->loop_start = gus_sample->data_length - tmp_loop;
		gus_sample->loop_fraction  = ((gus_sample->loop_fraction & 0x0f) << 4) | ((gus_sample->loop_fraction & 0xf0) >> 4);
		gus_sample->loop_start >>= 1;
		gus_sample->loop_end >>= 1;
		gus_sample->data_length >>= 1;
		gus_sample->modes ^= SAMPLE_REVERSE | SAMPLE_UNSIGNED;
		return 0;
	}
	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit unsigned reverse ping pong */
int
convert_16urp (unsigned char *data, struct _sample *gus_sample ) {
	unsigned long int loop_length = gus_sample->loop_end - gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data + gus_sample->data_length - 1;
	unsigned char *read_end = data + gus_sample->loop_end;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = calloc(((new_length >> 1) + 1), sizeof(signed short int));
	if (__builtin_expect((gus_sample->data != NULL),1)) {
		write_data = gus_sample->data;
		do {
			*write_data = ((*read_data--) ^ 0x80) << 8;
			*write_data |= *read_data--;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data < read_end);

		*write_data = ((*read_data--) ^ 0x80) << 8;
		*write_data |= *read_data--;
		write_data_a = write_data + (dloop_length >> 1);
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + (dloop_length >> 1);
		read_end = data + gus_sample->loop_start;
		do {
			*write_data = ((*read_data--) ^ 0x80) << 8;
			*write_data |= *read_data--;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			if (*write_data > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data;
			} else if (*write_data < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data;
			}
			write_data++;
		} while (read_data < read_end);

		*write_data = ((*read_data--) ^ 0x80) << 8;
		*write_data |= *read_data--;
		*write_data_b++ = *write_data;
		read_end = data - 1;
		do {
			*write_data_b = ((*read_data--) ^ 0x80) << 8;
			*write_data_b |= *read_data--;
			if (*write_data_b > gus_sample->max_peek) {
				gus_sample->max_peek = *write_data_b;
			} else if (*write_data_b < gus_sample->min_peek) {
				gus_sample->min_peek = *write_data_b;
			}
			write_data_b++;
		} while (read_data < read_end);
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_REVERSE | SAMPLE_UNSIGNED;
		return 0;
	}

	WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}


/* sample loading */

int
load_sample (struct _patch *sample_patch) {
	unsigned char *gus_patch;
	unsigned long int gus_size;
	unsigned long int gus_ptr;
	unsigned char no_of_samples;
	struct _sample *gus_sample = NULL;
	unsigned long int i = 0;

	int (*do_convert[])(unsigned char *data, struct _sample *gus_sample ) = {
		convert_8s,
		convert_16s,
		convert_8u,
		convert_16u,
		convert_8sp,
		convert_16sp,
		convert_8up,
		convert_16up,
		convert_8sr,
		convert_16sr,
		convert_8ur,
		convert_16ur,
		convert_8srp,
		convert_16srp,
		convert_8urp,
		convert_16urp
	};
	unsigned long int tmp_loop;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	SAMPLE_CONVERT_DEBUG(sample_patch->filename);
	sample_patch->loaded = 1;
	if ((gus_patch = WM_BufferFile(sample_patch->filename,&gus_size)) == NULL) {
		return -1;
	}
	if (gus_size < 239) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, sample_patch->filename, 0);
		free(gus_patch);
		return -1;
	}
	if (memcmp(gus_patch, "GF1PATCH110\0ID#000002", 22) && memcmp(gus_patch, "GF1PATCH100\0ID#000002", 22)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID,"(unsupported format)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, sample_patch->filename, 0);
		free(gus_patch);
		return -1;
	}
	if (gus_patch[82] > 1) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID,"(unsupported format)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, sample_patch->filename, 0);
		free(gus_patch);
		return -1;
	}
	if (gus_patch[151] > 1) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID,"(unsupported format)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, sample_patch->filename, 0);
		free(gus_patch);
		return -1;
	}

	no_of_samples = gus_patch[198];
	sample_patch->first_sample = NULL;
	gus_ptr = 239;
	while (no_of_samples) {
		unsigned long int tmp_cnt;
		if (sample_patch->first_sample == NULL) {
			sample_patch->first_sample = malloc(sizeof(struct _sample));
			gus_sample = sample_patch->first_sample;
		} else {
			gus_sample->next = malloc(sizeof(struct _sample));
			gus_sample = gus_sample->next;
		}
		if (gus_sample == NULL) {
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, 0);
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, sample_patch->filename, 0);
			return -1;
		}

		gus_sample->next = NULL;
		gus_sample->loop_fraction = gus_patch[gus_ptr+7];
		gus_sample->data_length = (gus_patch[gus_ptr+11] << 24) | (gus_patch[gus_ptr+10] << 16) | (gus_patch[gus_ptr+9] << 8) | gus_patch[gus_ptr+8];
		gus_sample->loop_start = (gus_patch[gus_ptr+15] << 24) | (gus_patch[gus_ptr+14] << 16) | (gus_patch[gus_ptr+13] << 8) | gus_patch[gus_ptr+12];
		gus_sample->loop_end = (gus_patch[gus_ptr+19] << 24) | (gus_patch[gus_ptr+18] << 16) | (gus_patch[gus_ptr+17] << 8) | gus_patch[gus_ptr+16];
		gus_sample->rate = (gus_patch[gus_ptr+21] << 8) | gus_patch[gus_ptr+20];
		gus_sample->freq_low = ((gus_patch[gus_ptr+25] << 24) | (gus_patch[gus_ptr+24] << 16) | (gus_patch[gus_ptr+23] << 8) | gus_patch[gus_ptr+22]);
		gus_sample->freq_high = ((gus_patch[gus_ptr+29] << 24) | (gus_patch[gus_ptr+28] << 16) | (gus_patch[gus_ptr+27] << 8) | gus_patch[gus_ptr+26]);
		gus_sample->freq_root = ((gus_patch[gus_ptr+33] << 24) | (gus_patch[gus_ptr+32] << 16) | (gus_patch[gus_ptr+31] << 8) | gus_patch[gus_ptr+30]);

		/* This is done this way instead of ((freq * 1024) / rate) to avoid 32bit overflow. */
		/* Result is 0.001% inacurate */
		gus_sample->inc_div = ((gus_sample->freq_root * 512) / gus_sample->rate) * 2;

#if 0
		printf("\rTremolo Sweep: %i, Rate: %i, Depth %i\n",
			gus_patch[gus_ptr+49], gus_patch[gus_ptr+50], gus_patch[gus_ptr+51]);
		printf("\rVibrato Sweep: %i, Rate: %i, Depth %i\n",
			gus_patch[gus_ptr+52], gus_patch[gus_ptr+53], gus_patch[gus_ptr+54]);
#endif
		gus_sample->modes = gus_patch[gus_ptr+55] & 0x7F;
		if ((sample_patch->remove & SAMPLE_SUSTAIN) && (gus_sample->modes & SAMPLE_SUSTAIN)) {
			gus_sample->modes ^= SAMPLE_SUSTAIN;
		}
		if (sample_patch->patchid & 0x0080) {
			if (!(sample_patch->keep & SAMPLE_LOOP)) {
 				gus_sample->modes &= 0xFB;
			}
			if (!(sample_patch->keep & SAMPLE_ENVELOPE)) {
				gus_sample->modes &= 0xBF;
			}
		}


		if (gus_sample->loop_start > gus_sample->loop_end) {
			tmp_loop = gus_sample->loop_end;
			gus_sample->loop_end = gus_sample->loop_start;
			gus_sample->loop_start = tmp_loop;
			gus_sample->loop_fraction  = ((gus_sample->loop_fraction & 0x0f) << 4) | ((gus_sample->loop_fraction & 0xf0) >> 4);
		}
		for (i = 0; i < 6; i++) {
			if (gus_sample->modes & SAMPLE_ENVELOPE) {
				unsigned char env_rate = gus_patch[gus_ptr+37+i];
				if (sample_patch->env[i].set & 0x02) {
					gus_sample->env_target[i] = 16448 * (unsigned long int)(255.0 * sample_patch->env[i].level);
				} else {
					gus_sample->env_target[i] = 16448 * gus_patch[gus_ptr+43+i];
				}

				if (sample_patch->env[i].set & 0x01) {
					gus_sample->env_rate[i]  = (unsigned long int)(4194303.0 / ((float)WM_SampleRate * (sample_patch->env[i].time / 1000.0)));
				} else {
					gus_sample->env_rate[i]  = (unsigned long int)(4194303.0 / ((float)WM_SampleRate * env_time_table[env_rate]));
					if (gus_sample->env_rate[i] == 0) {
						fprintf(stderr,"\rWarning: libWildMidi %s found invalid envelope(%lu) rate setting in %s. Using %f instead.\n",__FUNCTION__, i, sample_patch->filename, env_time_table[63]);
						gus_sample->env_rate[i]  = (unsigned long int)(4194303.0 / ((float)WM_SampleRate * env_time_table[63]));
					}
				}
			} else {
				gus_sample->env_target[i] = 4194303;
				gus_sample->env_rate[i]  = (unsigned long int)(4194303.0 / ((float)WM_SampleRate * env_time_table[63]));
			}
		}

		gus_sample->env_target[6] = 0;
		gus_sample->env_rate[6]  = (unsigned long int)(4194303.0 / ((float)WM_SampleRate * env_time_table[63]));

		if ((sample_patch->patchid == 47) && (!(gus_sample->modes & SAMPLE_LOOP))) {
			for (i = 3; i < 6; i++) {
				gus_sample->env_target[i] = gus_sample->env_target[2];
				gus_sample->env_rate[i] = gus_sample->env_rate[2];
			}
		}

		gus_ptr += 96;
		tmp_cnt = gus_sample->data_length;

/* convert to float */
		gus_sample->min_peek = 0;
		gus_sample->max_peek = 0;

		if (do_convert[(((gus_sample->modes & 0x18) >> 1)| (gus_sample->modes & 0x03))](&gus_patch[gus_ptr], gus_sample) == -1) {
			return -1;

		};

		if (gus_sample->max_peek > (-gus_sample->min_peek)) {
			gus_sample->peek_adjust = 33553408 / gus_sample->max_peek;
		} else {
			gus_sample->peek_adjust = 33554432 / (-gus_sample->min_peek);
		}
		gus_sample->peek_adjust = (gus_sample->peek_adjust * sample_patch->amp) >> 10;

		gus_ptr += tmp_cnt;
		gus_sample->loop_start = (gus_sample->loop_start << 10) | (((gus_sample->loop_fraction & 0x0f) << 10) / 16);
		gus_sample->loop_end = (gus_sample->loop_end << 10) | (((gus_sample->loop_fraction & 0xf0) << 6) / 16);
		gus_sample->loop_size = gus_sample->loop_end - gus_sample->loop_start;
		gus_sample->data_length = gus_sample->data_length << 10;
		no_of_samples--;
	}
	free(gus_patch);
	return 0;
}

#endif
static struct _patch *
get_patch_data(struct _mdi *mdi, unsigned short patchid) {
	struct _patch *search_patch;

	WM_Lock(&patch_lock);

	search_patch = patch[patchid & 0x007F];

	if (search_patch == NULL) {
		WM_Unlock(&patch_lock);
		return NULL;
	}

	while(search_patch != NULL) {
		if (search_patch->patchid == patchid) {
			WM_Unlock(&patch_lock);
			return search_patch;
		}
		search_patch = search_patch->next;
	}
	if ((patchid >> 8) != 0) {
		WM_Unlock(&patch_lock);
		return (get_patch_data(mdi, patchid & 0x00FF));
	}
	WM_Unlock(&patch_lock);
	return NULL;
}

static void
load_patch (struct _mdi *mdi, unsigned short patchid) {
	unsigned int i;
	struct _patch *tmp_patch = NULL;

	for (i = 0; i < mdi->patch_count; i++) {
		if (mdi->patches[i]->patchid == patchid) {
			return;
		}
	}

	tmp_patch = get_patch_data(mdi, patchid);
	if (tmp_patch == NULL) {
		return;
	}

	WM_Lock(&patch_lock);
	if (!tmp_patch->loaded) {
		if (load_sample(tmp_patch) == -1) {
			WM_Unlock(&patch_lock);
			return;
		}
	}

	if (tmp_patch->first_sample == NULL) {
		WM_Unlock(&patch_lock);
		return;
	}

	mdi->patch_count++;
	mdi->patches = realloc(mdi->patches, (sizeof(struct _patch) * mdi->patch_count));
	mdi->patches[mdi->patch_count -1] = tmp_patch;
	tmp_patch->inuse_count++;
	WM_Unlock(&patch_lock);
	return;
}


static struct _sample *
get_sample_data (struct _patch *sample_patch, unsigned long int freq) {
	struct _sample *last_sample = NULL;
	struct _sample *return_sample = NULL;

	WM_Lock(&patch_lock);
	if (sample_patch == NULL) {
		WM_Unlock(&patch_lock);
		return NULL;
	}
	if (sample_patch->first_sample == NULL) {
		WM_Unlock(&patch_lock);
		return NULL;
	}
	if (freq == 0) {
		WM_Unlock(&patch_lock);
		return sample_patch->first_sample;
	}

	return_sample = sample_patch->first_sample;
	last_sample = sample_patch->first_sample;
	while (last_sample != NULL) {
		if (freq > last_sample->freq_low) {
			if (freq < last_sample->freq_high) {
				WM_Unlock(&patch_lock);
				return last_sample;
			} else {
				return_sample = last_sample;
			}
		}
		last_sample = last_sample->next;
	}
	WM_Unlock(&patch_lock);
	return return_sample;
}

static void
do_note_off (struct _mdi *mdi, struct _event_data *data) {
	struct _note *nte;
	unsigned char ch = data->channel;

	MIDI_EVENT_DEBUG(__FUNCTION__,ch);

	nte = &mdi->note_table[0][ch][(data->data >> 8)];
	if (!nte->active)
		nte = &mdi->note_table[1][ch][(data->data >> 8)];
	if (!nte->active) {
		return;
	}

	if ((ch == 9) && (!(nte->modes & SAMPLE_LOOP))) {
		return;
	}

	if (nte->hold) {
		nte->hold |= HOLD_OFF;
	} else {
#if 0
		if (nte->modes & SAMPLE_SUSTAIN) {
			nte->env = 3;
			if (nte->env_level > nte->sample->env_target[3]) {
				nte->env_inc = -nte->sample->env_rate[3];
			} else {
				nte->env_inc = nte->sample->env_rate[3];
			}
		} else
#endif
		{
			if (nte->env < 4) {
				nte->env = 4;
				if (nte->env_level > nte->sample->env_target[4]) {
					nte->env_inc = -nte->sample->env_rate[4];
				} else {
					nte->env_inc = nte->sample->env_rate[4];
				}
			}
		}
	}
	return;
}

static inline unsigned long int
get_inc (struct _mdi *mdi, struct _note *nte) {
	int ch = nte->noteid >> 8;
	signed long int note_f;
	unsigned long int freq;

	if (__builtin_expect((nte->patch->note != 0),0)) {
		note_f = nte->patch->note * 100;
	} else {
		note_f = (nte->noteid & 0x7f) * 100;
	}
	note_f += mdi->channel[ch].pitch_adjust;
	if (__builtin_expect((note_f < 0), 0)) {
		note_f = 0;
	} else if (__builtin_expect((note_f > 12700), 0)) {
		note_f = 12700;
	}
	freq = freq_table[(note_f % 1200)] >> (10 - (note_f / 1200));
	return (((freq / ((WM_SampleRate * 100) / 1024)) * 1024 / nte->sample->inc_div));
}

static inline unsigned long int
get_volume(struct _mdi *mdi, unsigned char ch, struct _note *nte) {
	signed long int volume;

    if (mdi->info.mixer_options & WM_MO_LOG_VOLUME) {
 		volume = (sqr_volume[mdi->channel[ch].volume] *
		sqr_volume[mdi->channel[ch].expression] *
		sqr_volume[nte->velocity]) / 1048576;
	} else {
		volume = (lin_volume[mdi->channel[ch].volume] *
		lin_volume[mdi->channel[ch].expression] *
		lin_volume[nte->velocity]) / 1048576;
    }
	return ((volume * nte->sample->peek_adjust) >> 10);
}

static void
do_note_on (struct _mdi *mdi, struct _event_data *data) {
	struct _note *nte;
	unsigned long int freq = 0;
	struct _patch *patch;
	struct _sample *sample;
	unsigned char ch = data->channel;
	unsigned char note = (data->data >> 8);
	unsigned char velocity = (data->data & 0xFF);


	if (velocity == 0x00) {
	    do_note_off(mdi, data);
		return;
	}

	MIDI_EVENT_DEBUG(__FUNCTION__,ch);

	if (ch != 9) {
		patch = mdi->channel[ch].patch;
		if (patch == NULL) {
			return;
		}
		freq = freq_table[(note % 12) * 100] >> (10 -(note / 12));
	} else {
		patch = get_patch_data(mdi, ((mdi->channel[ch].bank << 8) | note | 0x80));
 		if (patch == NULL) {
			return;
		}
		if (patch->note) {
			freq = freq_table[(patch->note % 12) * 100] >> (10 -(patch->note / 12));
		} else {
			freq = freq_table[(note % 12) * 100] >> (10 -(note / 12));
		}
	}

	sample = get_sample_data(patch, (freq / 100));

	if (sample == NULL) {
		return;
	}

	nte = &mdi->note_table[0][ch][note];

	if (nte->active) {
		if ((nte->modes & SAMPLE_ENVELOPE) && (nte->env < 3) && (!(nte->hold & HOLD_OFF)))
			return;
		nte->next = &mdi->note_table[1][ch][note];
		nte->env = 6;
		nte->env_inc = -nte->sample->env_rate[6];
		nte = &mdi->note_table[1][ch][note];
	} else {
		if (mdi->note_table[1][ch][note].active) {
			if ((nte->modes & SAMPLE_ENVELOPE) && (nte->env < 3) && (!(nte->hold & HOLD_OFF)))
				return;
			mdi->note_table[1][ch][note].next = nte;
			mdi->note_table[1][ch][note].env = 6;
			mdi->note_table[1][ch][note].env_inc = -mdi->note_table[1][ch][note].sample->env_rate[6];
        } else {
			*mdi->last_note = nte;
			mdi->last_note++;
			nte->active = 1;
		}
	}
	nte->noteid = (ch << 8) | note;
	nte->patch = patch;
	nte->sample = sample;
	nte->sample_pos = 0;
	nte->sample_inc = get_inc (mdi, nte);
	nte->velocity = velocity;;
	nte->env = 0;
	nte->env_inc = nte->sample->env_rate[0];
	nte->env_level = 0;
	nte->modes = sample->modes;
	nte->hold = mdi->channel[ch].hold;
	nte->vol_lvl = get_volume(mdi, ch, nte);
	nte->next = NULL;
}

static void
do_aftertouch (struct _mdi *mdi, struct _event_data *data) {
	struct _note *nte;
	unsigned char ch = data->channel;

	MIDI_EVENT_DEBUG(__FUNCTION__,ch);

	nte = &mdi->note_table[0][ch][(data->data >> 8)];
    if (!nte->active) {
		nte = &mdi->note_table[1][ch][(data->data >> 8)];
		if (!nte->active) {
			return;
		}
	}

	nte->velocity = data->data & 0xff;
	nte->vol_lvl = get_volume(mdi, ch, nte);

	if (nte->next) {
		nte->next->velocity = data->data & 0xff;
		nte->next->vol_lvl = get_volume(mdi, ch, nte->next);
	}
}


static void
do_pan_adjust (struct _mdi *mdi, unsigned char ch) {
	signed short int pan_adjust = mdi->channel[ch].balance + mdi->channel[ch].pan;
	signed long int left, right;

	if (pan_adjust > 63) {
		pan_adjust = 63;
	} else if (pan_adjust < -64) {
		pan_adjust = -64;
	}

	pan_adjust += 64;
	if (mdi->info.mixer_options & WM_MO_LOG_VOLUME) {
 		left =  (pan_volume[127 - pan_adjust] * WM_MasterVolume * mdi->amp) / 1048576;
 		right = (pan_volume[pan_adjust] * WM_MasterVolume * mdi->amp) / 1048576;
	} else {
        left = (lin_volume[127 - pan_adjust] * WM_MasterVolume * mdi->amp) / 1048576;
		right= (lin_volume[pan_adjust] * WM_MasterVolume * mdi->amp) / 1048576;
 	}

	mdi->channel[ch].left_adjust = left;
	mdi->channel[ch].right_adjust = right;
}

static void
do_control_bank_select (struct _mdi *mdi, struct _event_data *data)
{
    unsigned char ch = data->channel;
    mdi->channel[ch].bank = data->data;
}

static void
do_control_data_entry_course (struct _mdi *mdi, struct _event_data *data)
{
    unsigned char ch = data->channel;
	int data_tmp;

	if (mdi->channel[ch].reg_data == 0x0000) { // Pitch Bend Range
		data_tmp = mdi->channel[ch].pitch_range % 100;
		mdi->channel[ch].pitch_range = data->data * 100 + data_tmp;
	}
}

static void
do_control_channel_volume (struct _mdi *mdi, struct _event_data *data)
{
	struct _note **note_data = mdi->note;
    unsigned char ch = data->channel;

	mdi->channel[ch].volume = data->data;

	if (note_data != mdi->last_note) {
		do {
			if (((*note_data)->noteid >> 8) == ch) {
				(*note_data)->vol_lvl = get_volume(mdi, ch, *note_data);
				if ((*note_data)->next)
					(*note_data)->next->vol_lvl = get_volume(mdi, ch, (*note_data)->next);
			}
			note_data++;
		} while (note_data != mdi->last_note);
	}
}

static void
do_control_channel_balance (struct _mdi *mdi, struct _event_data *data)
{
    unsigned char ch = data->channel;

	mdi->channel[ch].balance = data->data - 64;
	do_pan_adjust(mdi, ch);
}

static void
do_control_channel_pan (struct _mdi *mdi, struct _event_data *data)
{
    unsigned char ch = data->channel;

	mdi->channel[ch].pan = data->data - 64;
	do_pan_adjust(mdi, ch);
}

static void
do_control_channel_expression (struct _mdi *mdi, struct _event_data *data)
{
	struct _note **note_data = mdi->note;
    unsigned char ch = data->channel;

    mdi->channel[ch].expression = data->data;

	if (note_data != mdi->last_note) {
		do {
			if (((*note_data)->noteid >> 8) == ch) {
    			(*note_data)->vol_lvl = get_volume(mdi, ch, *note_data);
				if ((*note_data)->next)
					(*note_data)->next->vol_lvl = get_volume(mdi, ch, (*note_data)->next);
			}
			note_data++;
		} while (note_data != mdi->last_note);
	}
}

static void
do_control_data_entry_fine (struct _mdi *mdi, struct _event_data *data)
{
    unsigned char ch = data->channel;
	int data_tmp;

	if (mdi->channel[ch].reg_data == 0x0000) { // Pitch Bend Range
		data_tmp = mdi->channel[ch].pitch_range / 100;
		mdi->channel[ch].pitch_range = (data_tmp * 100) + data->data;
	}

}

static void
do_control_channel_hold (struct _mdi *mdi, struct _event_data *data)
{
	struct _note **note_data = mdi->note;
    unsigned char ch = data->channel;

	if (data->data > 63) {
		mdi->channel[ch].hold = 1;
	} else {
		mdi->channel[ch].hold = 0;
		if (note_data != mdi->last_note) {
			do {
				if (((*note_data)->noteid >> 8) == ch) {
					if ((*note_data)->hold & HOLD_OFF) {
						if ((*note_data)->modes & SAMPLE_ENVELOPE) {
							if ((*note_data)->env < 4) {
								(*note_data)->env = 4;
								if ((*note_data)->env_level > (*note_data)->sample->env_target[4]) {
									(*note_data)->env_inc = -(*note_data)->sample->env_rate[4];
								} else {
									(*note_data)->env_inc = (*note_data)->sample->env_rate[4];
								}
							}
						}
    				}
	    			(*note_data)->hold = 0x00;
		    	}
    			note_data++;
    	  	} while (note_data != mdi->last_note);
    	}
    }
}

static void
do_control_data_increment (struct _mdi *mdi, struct _event_data *data)
{
    unsigned char ch = data->channel;

	if (mdi->channel[ch].reg_data == 0x0000) { // Pitch Bend Range
	    if (mdi->channel[ch].pitch_range < 0x3FFF)
    		mdi->channel[ch].pitch_range++;
	}
}

static void
do_control_data_decrement (struct _mdi *mdi, struct _event_data *data)
{
    unsigned char ch = data->channel;

	if (mdi->channel[ch].reg_data == 0x0000) { // Pitch Bend Range
	    if (mdi->channel[ch].pitch_range > 0)
    		mdi->channel[ch].pitch_range--;
	}
}

static void
do_control_registered_param_fine (struct _mdi *mdi, struct _event_data *data)
{
    unsigned char ch = data->channel;
	mdi->channel[ch].reg_data = (mdi->channel[ch].reg_data & 0x3F80) | data->data;
}

static void
do_control_registered_param_course (struct _mdi *mdi, struct _event_data *data)
{
    unsigned char ch = data->channel;
	mdi->channel[ch].reg_data = (mdi->channel[ch].reg_data & 0x7F) | (data->data << 7);
}

static void
do_control_channel_sound_off (struct _mdi *mdi, struct _event_data *data)
{
	struct _note **note_data = mdi->note;
    unsigned char ch = data->channel;

	if (note_data != mdi->last_note) {
		do {
			if (((*note_data)->noteid >> 8) == ch) {
				(*note_data)->active = 0;
				if ((*note_data)->next) {
					(*note_data)->next = NULL;
				}
			}
			note_data++;
		} while (note_data != mdi->last_note);
		mdi->last_note = mdi->note;
	}

}

static void
do_control_channel_controllers_off (struct _mdi *mdi, struct _event_data *data)
{
	struct _note **note_data = mdi->note;
    unsigned char ch = data->channel;

	mdi->channel[ch].expression = 127;
	mdi->channel[ch].pressure = 0;
	mdi->channel[ch].volume = 100;
	mdi->channel[ch].pan = 0;
	mdi->channel[ch].balance = 0;
	mdi->channel[ch].reg_data = 0xffff;
	mdi->channel[ch].pitch_range = 200;
	mdi->channel[ch].pitch = 0;
	mdi->channel[ch].pitch_adjust = 0;
	mdi->channel[ch].hold = 0;
	do_pan_adjust(mdi, ch);

	if (note_data != mdi->last_note) {
		do {
			if (((*note_data)->noteid >> 8 ) == ch) {
				(*note_data)->sample_inc = get_inc (mdi, *note_data);
				(*note_data)->velocity = 0;
				(*note_data)->vol_lvl = get_volume(mdi, ch, *note_data);
				(*note_data)->hold = 0;

				if ((*note_data)->next) {
					(*note_data)->next->velocity = data->data;
					(*note_data)->next->vol_lvl = get_volume(mdi, ch, (*note_data)->next);
				}

			}
			note_data++;
		} while (note_data != mdi->last_note);
	}
}

static void
do_control_channel_notes_off (struct _mdi *mdi, struct _event_data *data)
{
	struct _note **note_data = mdi->note;
    unsigned char ch = data->channel;

	if (ch == 9)
		return;
	if (note_data != mdi->last_note) {
		do {
			if (((*note_data)->noteid >> 8) == ch) {
				if (!(*note_data)->hold){
					if ((*note_data)->modes & SAMPLE_ENVELOPE) {
						if ((*note_data)->env < 5) {
							if ((*note_data)->env_level > (*note_data)->sample->env_target[5]) {
								(*note_data)->env_inc = -(*note_data)->sample->env_rate[5];
							} else {
								(*note_data)->env_inc = (*note_data)->sample->env_rate[5];
							}
							(*note_data)->env = 5;
						}
					}
				} else {
					(*note_data)->hold |= HOLD_OFF;
				}
			}
			note_data++;
		} while (note_data != mdi->last_note);
	}
}

static void
do_patch (struct _mdi *mdi, struct _event_data *data) {
    unsigned char ch = data->channel;
	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
	if (ch != 9) {
        mdi->channel[ch].patch = get_patch_data(mdi, ((mdi->channel[ch].bank << 8) | data->data));
	} else {
		mdi->channel[ch].bank = data->data;
	}
}

static void
do_channel_pressure (struct _mdi *mdi, struct _event_data *data) {
	struct _note **note_data = mdi->note;
	unsigned char ch = data->channel;

	MIDI_EVENT_DEBUG(__FUNCTION__,ch);

	if (note_data != mdi->last_note) {
		do {
			if (((*note_data)->noteid >> 8 ) == ch) {
				(*note_data)->velocity = data->data;
				(*note_data)->vol_lvl = get_volume(mdi, ch, *note_data);

				if ((*note_data)->next) {
					(*note_data)->next->velocity = data->data;
					(*note_data)->next->vol_lvl = get_volume(mdi, ch, (*note_data)->next);
				}
			}
			note_data++;
		} while (note_data != mdi->last_note);
	}
}

static void
do_pitch (struct _mdi *mdi, struct _event_data *data) {
	struct _note **note_data = mdi->note;
    unsigned char ch = data->channel;

	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
	mdi->channel[ch].pitch = data->data - 0x2000;

	if (mdi->channel[ch].pitch < 0) {
		mdi->channel[ch].pitch_adjust = mdi->channel[ch].pitch_range * mdi->channel[ch].pitch / 8192;
	} else {
		mdi->channel[ch].pitch_adjust = mdi->channel[ch].pitch_range * mdi->channel[ch].pitch / 8191;
	}

	if (note_data != mdi->last_note) {
		do {
			if (((*note_data)->noteid >> 8 ) == ch) {
				(*note_data)->sample_inc = get_inc (mdi, *note_data);
			}
			note_data++;
		} while (note_data != mdi->last_note);
	}
}
#if 0
static void
do_message (unsigned char ch, struct _mdi *mdi, unsigned long int ptr) {
	unsigned char event_type = 0xF0 | ch;
	static unsigned long int tempo = 500000;

	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
	if (event_type == 0xFF) {
		if ((mdi->data[ptr] == 0x51) && (mdi->data[ptr+1] == 3)) {
			tempo = (mdi->data[ptr+2] << 16) | (mdi->data[ptr+3] << 8) | mdi->data[ptr+4];
			if (tempo == 0)
				mdi->samples_per_delta = (WM_SampleRate << 10) / (2 * mdi->divisions);
			else
				mdi->samples_per_delta = (WM_SampleRate << 10) / ((1000000 * mdi->divisions) / tempo);
		}
	}
}

static void
do_null (unsigned char ch, struct _mdi *mdi, unsigned long int ptr) {
	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
};
#endif

static void
WM_ResetToStart(midi * handle) {
	struct _mdi *mdi = (struct _mdi *)handle;
	int i;

    mdi->current_event = mdi->events;
	mdi->samples_to_mix = 0;
	mdi->info.current_sample= 0;

	for (i=0; i<16; i++) {
		mdi->channel[i].bank = 0;
		mdi->channel[i].patch = NULL;
		mdi->channel[i].hold = 0;
		mdi->channel[i].volume = 100;
		mdi->channel[i].pressure = 127;
		mdi->channel[i].expression = 127;
		mdi->channel[i].balance = 0;
		mdi->channel[i].pan = 0;
		mdi->channel[i].left_adjust = 1.0;
		mdi->channel[i].right_adjust = 1.0;
		mdi->channel[i].pitch = 0;
		mdi->channel[i].pitch_range = 200;
		mdi->channel[i].reg_data = 0xFFFF;
		do_pan_adjust(mdi, i);
	}
}

static int
midi_setup_noteoff (struct _mdi *mdi, unsigned char channel, unsigned char note, unsigned char velocity)
{
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = *do_note_off;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = (note << 8) | velocity;
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = *do_note_off;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = (note << 8) | velocity;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }
    return 0;
}

static int
midi_setup_noteon (struct _mdi *mdi, unsigned char channel, unsigned char note, unsigned char velocity)
{
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = *do_note_on;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = (note << 8) | velocity;
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = *do_note_on;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = (note << 8) | velocity;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }

    if ((lin_volume[mdi->channel[channel].volume] * lin_volume[velocity]) > mdi->lin_max_vol)
        mdi->lin_max_vol = lin_volume[mdi->channel[channel].volume] * lin_volume[velocity];

    if ((sqr_volume[mdi->channel[channel].volume] * sqr_volume[velocity]) > mdi->log_max_vol)
        mdi->log_max_vol = sqr_volume[mdi->channel[channel].volume] * sqr_volume[velocity];

    if (channel == 9)
        load_patch(mdi, ((mdi->channel[channel].bank << 8) | (note | 0x80)));
    return 0;
}

static int
midi_setup_aftertouch (struct _mdi *mdi, unsigned char channel, unsigned char note, unsigned char pressure)
{
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = *do_aftertouch;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = (note << 8) | pressure;
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = *do_aftertouch;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = (note << 8) | pressure;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }
    return 0;
}

static int
midi_setup_control (struct _mdi *mdi, unsigned char channel, unsigned char controller, unsigned char setting)
{
    void (*tmp_event)(struct _mdi *mdi, struct _event_data *data) = NULL;

    switch(controller)
    {
        case 0:
            tmp_event = *do_control_bank_select;
            mdi->channel[channel].bank = setting;
        break;
        case 6:
            tmp_event = *do_control_data_entry_course;
            break;
        case 7:
            tmp_event = *do_control_channel_volume;
            mdi->channel[channel].volume = setting;
            break;
        case 8:
            tmp_event = *do_control_channel_balance;
            break;
        case 10:
            tmp_event = *do_control_channel_pan;
            break;
        case 11:
            tmp_event = *do_control_channel_expression;
            break;
        case 38:
            tmp_event = *do_control_data_entry_fine;
            break;
        case 64:
            tmp_event = *do_control_channel_hold;
            break;
        case 96:
            tmp_event = *do_control_data_increment;
            break;
        case 97:
            tmp_event = *do_control_data_decrement;
            break;
        case 100:
            tmp_event = *do_control_registered_param_fine;
            break;
        case 101:
            tmp_event = *do_control_registered_param_course;
            break;
        case 120:
            tmp_event = *do_control_channel_sound_off;
            break;
        case 121:
            tmp_event = *do_control_channel_controllers_off;
            break;
        case 123:
            tmp_event = *do_control_channel_notes_off;
            break;
        default:
            return 0;
    }
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = tmp_event;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = setting;
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = tmp_event;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = setting;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }
    return 0;
}

static int
midi_setup_patch (struct _mdi *mdi, unsigned char channel, unsigned char patch)
{
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = *do_patch;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = patch;
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = *do_patch;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = patch;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }
    if (channel == 9)
    {
        mdi->channel[channel].bank = patch;
    } else {
        load_patch(mdi, ((mdi->channel[channel].bank << 8) | patch));
    }
    return 0;
}

static int
midi_setup_channel_pressure (struct _mdi *mdi, unsigned char channel, unsigned char pressure)
{

    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = *do_channel_pressure;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = pressure;
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = *do_channel_pressure;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = pressure;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }

    return 0;
}

static int
midi_setup_pitch (struct _mdi *mdi, unsigned char channel, unsigned short pitch)
{
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = *do_pitch;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = pitch;
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = *do_pitch;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = pitch;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }
    return 0;
}


static struct _mdi *
WM_ParseNewMidi (unsigned char *midi_data, unsigned int midi_size)
{
    struct _mdi *mdi;
    unsigned int tmp_val;
    unsigned int track_size;
    unsigned char **tracks;
    unsigned int end_of_tracks = 0;
    unsigned int no_tracks;
    unsigned int i;
    unsigned int divisions = 96;
    unsigned int tempo = 500000;
    unsigned int samples_per_delta = 0;
    unsigned long int sample_count = 0;
    unsigned long int sample_remainder = 0;

    struct _hndl *tmp_handle = NULL;

    unsigned long int *track_delta;
    unsigned char *track_end;
    unsigned long int smallest_delta = 0;
    unsigned long int subtract_delta = 0;
    unsigned long int tmp_length = 0;
    unsigned char current_event;
    unsigned char *running_event;


    mdi = malloc(sizeof (struct _mdi));
    memset(mdi, 0, (sizeof(struct _mdi)));

    if (first_handle == NULL) {
		first_handle = malloc(sizeof(struct _hndl));
		if (first_handle == NULL) {
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM," to parse midi data", errno);
			free(mdi);
			return NULL;
		}
		first_handle->handle = (void *)mdi;
		first_handle->prev = NULL;
		first_handle->next = NULL;
	} else {
		tmp_handle = first_handle;
		if (tmp_handle->next != NULL) {
			while (tmp_handle->next != NULL)
				tmp_handle = tmp_handle->next;
		}
		tmp_handle->next = malloc(sizeof(struct _hndl));
		if (tmp_handle->next == NULL) {
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM," to parse midi data", errno);
			free(mdi);
			return NULL;
		}
		tmp_handle->next->prev = tmp_handle;
		tmp_handle = tmp_handle->next;
		tmp_handle->next = NULL;
		tmp_handle->handle = (void *)mdi;
	}
    mdi->info.mixer_options = WM_MixerOptions;

   	load_patch(mdi, 0x0000);

	for (i=0; i<16; i++) {
		mdi->channel[i].volume = 100;
		mdi->channel[i].pressure = 127;
		mdi->channel[i].expression = 127;
		mdi->channel[i].pitch_range = 200;
		mdi->channel[i].reg_data = 0xFFFF;
		mdi->channel[i].patch = get_patch_data(mdi, 0x0000);
	}


    if (strncmp((char *) midi_data,"RIFF",4) == 0)
    {
		midi_data += 20;
		midi_size -= 20;
    }
    if (strncmp((char *) midi_data,"MThd",4) != 0) {
        printf("Not a midi file\n");
		free(mdi);
		return NULL;
	}
	midi_data += 4;
	midi_size -= 4;

    if (midi_size < 10)
    {
        printf("Midi File Too Short\n");
		free(mdi);
		return NULL;
    }

/*
 * Get Midi Header Size - must always be 6
 */
	tmp_val = *midi_data++ << 24;
	tmp_val |= *midi_data++ << 16;
	tmp_val |= *midi_data++ << 8;
	tmp_val |= *midi_data++;
    midi_size -= 4;
    if (tmp_val != 6)
    {
        printf("Corrupt Midi Header\n");
        free (mdi);
        return NULL;
    }

/*
 * Get Midi Format - we only support 0 and 1
 */
    tmp_val = *midi_data++ << 8;
	tmp_val |= *midi_data++;
    midi_size -= 2;
    if (tmp_val > 1)
    {
        printf("Midi Format Not Supported\n");
        free (mdi);
        return NULL;
    }

/*
 * Get No. of Tracks
 */
    tmp_val = *midi_data++ << 8;
	tmp_val |= *midi_data++;
    midi_size -= 2;
    if (tmp_val < 1)
    {
        printf("Midi Contains No Tracks\n");
        free (mdi);
        return NULL;
    }
    no_tracks = tmp_val;

/*
 * Get Divisions
 */
    divisions = *midi_data++ << 8;
	divisions |= *midi_data++;
    midi_size -= 2;
    if (divisions & 0x00008000)
    {
        printf("Division Type Note Supported\n");
        free (mdi);
        return NULL;
    }

    samples_per_delta = (WM_SampleRate << 10) / ((1000000 * divisions) / tempo);
    tracks = malloc (sizeof(char *) * no_tracks);
    track_delta = malloc (sizeof(unsigned long int) * no_tracks);
    track_end = malloc (sizeof(unsigned char) * no_tracks);
    running_event = malloc (sizeof(unsigned char) * no_tracks);
    smallest_delta = 0;

    for (i = 0; i < no_tracks; i++)
    {
        if (midi_size < 8)
        {
            printf("Midi File Too Short\n");
            free(tracks);
    		free(mdi);
    		return NULL;
        }

        if (strncmp((char *) midi_data,"MTrk",4) != 0) {
            printf("Expected Track Header\n");
            free(tracks);
    		free(mdi);
    		return NULL;
    	}
    	midi_data += 4;
    	midi_size -= 4;

        track_size = *midi_data++ << 24;
        track_size |= *midi_data++ << 16;
        track_size |= *midi_data++ << 8;
        track_size |= *midi_data++;
        midi_size -= 4;
        if (midi_size < track_size)
        {
            printf("Midi File Too Short\n");
            free(tracks);
    		free(mdi);
    		return NULL;
        }
        if ((midi_data[track_size-3] != 0xFF) || (midi_data[track_size-2] != 0x2F) || (midi_data[track_size-1] != 0x00))
        {
            printf("Corrupt Midi, Expected EOT\n");
            free(tracks);
            free(mdi);
            return NULL;
        }
        tracks[i] = midi_data;
        midi_data += track_size;
        midi_size -= track_size;
        track_end[i] = 0;
        running_event[i] = 0;
        track_delta[i] = 0;
        while (*tracks[i] > 0x7F)
        {
              track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
              tracks[i]++;
        }
        track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
        tracks[i]++;
    }

    while (end_of_tracks != no_tracks)
    {
        smallest_delta = 0;
        for (i = 0; i < no_tracks; i++)
        {
            if (track_end[i])
                continue;

            if (track_delta[i])
            {
                track_delta[i] -= subtract_delta;
                if (track_delta[i])
                {
                    if ((!smallest_delta) || (smallest_delta > track_delta[i]))
                    {
                        smallest_delta = track_delta[i];
                    }
                    continue;
                }
            }

            do {
                if (*tracks[i] > 0x7F)
                {
                    current_event = *tracks[i];
                    tracks[i]++;
                } else {
                    current_event = running_event[i];
                    if (running_event[i] < 0x80)
                    {
                        printf("Invalid Data in Midi, Expected Event\n");
                        free (tracks);
                        free (track_end);
                        free (track_delta);
                        if (mdi->events)
                            free (mdi->events);
                        free (mdi);
                        return NULL;
                    }
                }
                switch (current_event >> 4)
                {
                    case 0x8:
                        midi_setup_noteoff (mdi, (current_event & 0x0F), tracks[i][0], tracks[i][1]);
                        tracks[i] += 2;
                        running_event[i] = current_event;
                        break;
                    case 0x9:
                        midi_setup_noteon (mdi, (current_event & 0x0F), tracks[i][0], tracks[i][1]);
                        tracks[i] += 2;
                        running_event[i] = current_event;
                        break;
                    case 0xA:
                        midi_setup_aftertouch (mdi, (current_event & 0x0F), tracks[i][0], tracks[i][1]);
                        tracks[i] += 2;
                        running_event[i] = current_event;
                        break;
                    case 0xB:
                        midi_setup_control (mdi, (current_event & 0x0F), tracks[i][0], tracks[i][1]);
                        tracks[i] += 2;
                        running_event[i] = current_event;
                        break;
                    case 0xC:
                        midi_setup_patch (mdi,  (current_event & 0x0F), *tracks[i]);
                        tracks[i]++;
                        running_event[i] = current_event;
                        break;
                    case 0xD:
                        midi_setup_channel_pressure (mdi,  (current_event & 0x0F), *tracks[i]);
                        tracks[i]++;
                        running_event[i] = current_event;
                        break;
                    case 0xE:
                        midi_setup_pitch (mdi, (current_event & 0x0F), ((tracks[i][1] << 7) | (tracks[i][0] & 0x7F)));
                        tracks[i] += 2;
                        running_event[i] = current_event;
                        break;
                    case 0xF:
                        if (current_event == 0xFF)
                        {
                            if ((tracks[i][0] == 0x2F) && (tracks[i][1] == 0x00))
                            {
                                end_of_tracks++;
                                track_end[i] = 1;
                                goto NEXT_TRACK;
                            } else if ((tracks[i][0] == 0x51) && (tracks[i][1] == 0x03))
                            {
                                tempo = (tracks[i][2] << 16) + (tracks[i][3] << 8) + tracks[i][4];
                                tracks[i] += 5;
                                if (!tempo)
                                    samples_per_delta = (WM_SampleRate << 10) / (2 * divisions);
                                else
                                    samples_per_delta = (WM_SampleRate << 10) / ((1000000 * divisions) / tempo);
                            } else {
                                tmp_length = 0;
                                tracks[i]++;
                                while (*tracks[i] > 0x7f)
                                {
                                    tmp_length = (tmp_length << 7) + (*tracks[i] & 0x7f);
                                    tracks[i]++;
                                }
                                tmp_length = (tmp_length << 7) + (*tracks[i] & 0x7f);
                                tracks[i] += tmp_length + 1;
                            }
                        } else if (current_event == 0xF0)
                        {
                            running_event[i] = 0;
                            while (*tracks[i] != 0xF7)
                            {
                                tracks[i]++;
                            }
                            tracks[i]++;
                        } else {
                            printf("Um, WTF is this?\n");
                            free (tracks);
                            free (track_end);
                            free (track_delta);
                            if (mdi->events)
                                free (mdi->events);
                            free (mdi);
                            return NULL;
                        }
                        break;
                    default:
                        printf("Should Never of Gotten Here\n");
                        free (tracks);
                        free (track_end);
                        free (track_delta);
                        if (mdi->events)
                            free (mdi->events);
                        free (mdi);
                        return NULL;
                }
                while (*tracks[i] > 0x7F)
                {
                    track_delta[i] = (track_delta[i] << 7) + (*tracks[i] &0x7F);
                    tracks[i]++;
                }
                track_delta[i] = (track_delta[i] << 7) + (*tracks[i] &0x7F);
                tracks[i]++;
            } while (!track_delta[i]);
            if ((!smallest_delta) || (smallest_delta > track_delta[i]))
            {
                smallest_delta = track_delta[i];
            }
            NEXT_TRACK:
            continue;
        }

        subtract_delta = smallest_delta;
        sample_count = ((smallest_delta * samples_per_delta) + sample_remainder) >> 10;
        sample_remainder = sample_count & 0x3FF;
        if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
        {
            mdi->events[mdi->event_count - 1].samples_to_next += sample_count;
        } else {
            mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
            mdi->events[mdi->event_count].do_event = NULL;
            mdi->events[mdi->event_count].event_data.channel = 0;
            mdi->events[mdi->event_count].event_data.data = 0;
            mdi->events[mdi->event_count].samples_to_next = sample_count;
            mdi->event_count++;
        }
        mdi->info.approx_total_samples += sample_count;
    }
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->info.approx_total_samples -= mdi->events[mdi->event_count - 1].samples_to_next;
        mdi->event_count--;
    }
    mdi->info.approx_total_samples += WM_SampleRate * 3;

    if ((mdi->reverb = init_reverb(WM_SampleRate)) == NULL) {
		printf("Reverb Init Failed\n");
		free (tracks);
        free (track_end);
        free (track_delta);
        if (mdi->events)
            free (mdi->events);
        free (mdi);
        return NULL;
	}

    mdi->info.current_sample = 0;
    mdi->current_event = &mdi->events[0];
    mdi->samples_to_mix = 0;
    mdi->last_note = mdi->note;

	if (mdi->info.mixer_options & WM_MO_LOG_VOLUME)
    {
		mdi->amp = (281 * ((mdi->lin_max_vol << 10) / mdi->log_max_vol)) >> 10;
	} else {
		mdi->amp = 281;
	}
    mdi->amp = (mdi->amp * (((lin_volume[127] * lin_volume[127]) << 10) / mdi->lin_max_vol)) >> 10;

    WM_ResetToStart(mdi);

    free (track_end);
    free (track_delta);
    free (tracks);
    return mdi;
}

/*
 * =========================
 * External Functions
 * =========================
 */

const char *
WildMidi_GetString (unsigned short int info) {
	switch (info) {
		case WM_GS_VERSION:
			return WM_Version;
	}
	return NULL;
}

int
WildMidi_Init (const char * config_file, unsigned short int rate, unsigned short int options) {
	if (WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_ALR_INIT, NULL, 0);
		return -1;
	}

	if (config_file == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL config file pointer)", 0);
		return -1;
	}
	WM_InitPatches();
	if (WM_LoadConfig(config_file) == -1) {
		return -1;
	}

	if (options & 0xFFD8) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid option)", 0);
		WM_FreePatches();
		return -1;
	}
	WM_MixerOptions = options;

	if ((rate < 11000) || (rate > 65000)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(rate out of bounds, range is 11000 - 65000)", 0);
		WM_FreePatches();
		return -1;
	}
	WM_SampleRate = rate;
	WM_Initialized = 1;
	patch_lock = 0;

	init_gauss();
	return 0;
}

int
WildMidi_MasterVolume (unsigned char master_volume) {
	struct _mdi *mdi = NULL;
	struct _hndl * tmp_handle = first_handle;
	int i = 0;

	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (master_volume > 127) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(master volume out of range, range is 0-127)", 0);
		return -1;
	}

	WM_MasterVolume = lin_volume[master_volume];

	if (tmp_handle != NULL) {
		while(tmp_handle != NULL) {
			mdi = (struct _mdi *)tmp_handle->handle;
			for (i = 0; i < 16; i++) {
				do_pan_adjust(mdi, i);
			}
			tmp_handle = tmp_handle->next;
		}
	}

	return 0;
}

int
WildMidi_Close (midi * handle) {
	struct _mdi *mdi = (struct _mdi *)handle;
	struct _hndl * tmp_handle;
	struct _sample *tmp_sample;
	unsigned long int i;

	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (handle == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
		return -1;
	}
	if (first_handle == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(no midi's open)", 0);
		return -1;
	}
	WM_Lock(&mdi->lock);
	if (first_handle->handle == handle) {
		tmp_handle = first_handle->next;
		free (first_handle);
		first_handle = tmp_handle;
		if (first_handle != NULL)
			first_handle->prev = NULL;
	} else {
		tmp_handle = first_handle;
		while (tmp_handle->handle != handle) {
			tmp_handle = tmp_handle->next;
			if (tmp_handle == NULL) {
				WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(handle does not exist)", 0);
				return -1;
			}
		}
		tmp_handle->prev->next = tmp_handle->next;
		if (tmp_handle->next != NULL) {
			tmp_handle->next->prev = tmp_handle->prev;
		}
		free (tmp_handle);
	}

	if (mdi->patch_count != 0) {
		WM_Lock(&patch_lock);
		for (i = 0; i < mdi->patch_count; i++) {
			mdi->patches[i]->inuse_count--;
			if (mdi->patches[i]->inuse_count == 0) {
				//free samples here
				if (mdi->patches[i]->first_sample != NULL) {
					while (mdi->patches[i]->first_sample != NULL) {
						tmp_sample = mdi->patches[i]->first_sample->next;
						if (mdi->patches[i]->first_sample->data)
							free(mdi->patches[i]->first_sample->data);
						free(mdi->patches[i]->first_sample);
						mdi->patches[i]->first_sample = tmp_sample;
					}
					mdi->patches[i]->loaded = 0;
				}
			}
		}
		WM_Unlock(&patch_lock);
		free (mdi->patches);
	}

	if (mdi->events != NULL) {
		free (mdi->events);
	}

	if (mdi->tmp_info != NULL) {
		free (mdi->tmp_info);
	}
    free_reverb(mdi->reverb);
	free (mdi);
	// no need to unlock cause the struct containing the lock no-longer exists;
	return 0;
}

midi *
WildMidi_Open (const char *midifile) {
	unsigned char *mididata = NULL;
	unsigned long int midisize = 0;

	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return NULL;
	}
	if (midifile == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL filename)", 0);
		return NULL;
	}

	if ((mididata = WM_BufferFile(midifile, &midisize)) == NULL) {
		return NULL;
	}

	return (void *)WM_ParseNewMidi(mididata,midisize);
}

midi *
WildMidi_OpenBuffer (unsigned char *midibuffer, unsigned long int size) {
	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return NULL;
	}
	if (midibuffer == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL midi data buffer)", 0);
		return NULL;
	}

	return (void *)WM_ParseNewMidi(midibuffer,size);
}

int
WildMidi_LoadSamples( midi * handle) {
	return 0;
}

int
WildMidi_FastSeek ( midi * handle, unsigned long int *sample_pos) {
#if 0
	struct _mdi *mdi = (struct _mdi *)handle;
	struct _note **note_data = mdi->note;
	void (*do_event[])(unsigned char ch, struct _mdi *midifile, unsigned long int ptr) = {
		*do_null,
		*do_null,
		*do_aftertouch,
		*do_control,
		*do_patch,
		*do_channel_pressure,
		*do_pitch,
		*do_message
	};
	unsigned long int real_samples_to_mix = 0;

	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (handle == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
		return -1;
	}
	WM_Lock(&mdi->lock);
	if (sample_pos == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL seek position pointer)", 0);
		WM_Unlock(&mdi->lock);
		return -1;
	}

	if (*sample_pos == mdi->info.current_sample) {
		WM_Unlock(&mdi->lock);
		return 0;
	}

	if (*sample_pos > mdi->info.current_sample) {
		if ((mdi->sample_count == 0) && (mdi->index_count == mdi->index_size) && (mdi->last_note == 0)) {
			*sample_pos = mdi->info.current_sample;
			WM_Unlock(&mdi->lock);
			return 0;
		}
	} else {
		WM_ResetToStart(handle);
	}

	//reset all notes
	if (note_data != mdi->last_note) {
		do {
			(*note_data)->active = 0;
			*note_data = NULL;
			note_data++;
		} while (note_data != mdi->last_note);
		mdi->last_note = mdi->note;
	}

	while (*sample_pos != mdi->info.current_sample) {
		if (!mdi->sample_count) {
			if (mdi->index_count != mdi->index_size) {

				do {
					if (mdi->index_count == mdi->index_size) {
						break;
					}

					if (mdi->index_count != 0) {
						do_event[((mdi->index[mdi->index_count].event & 0xF0) >> 4) - 8]((mdi->index[mdi->index_count].event & 0x0F), mdi, mdi->index[mdi->index_count].offset);
					}
				} while (mdi->index[mdi->index_count++].delta == 0);

				mdi->samples_to_mix += mdi->index[mdi->index_count-1].delta * mdi->samples_per_delta;
				mdi->sample_count = mdi->samples_to_mix >> 10;
				mdi->samples_to_mix %= 1024;
			} else {
				mdi->sample_count = WM_SampleRate;
			}
		}

		if (mdi->sample_count <= (*sample_pos - mdi->info.current_sample)) {
			real_samples_to_mix = mdi->sample_count;
			if (real_samples_to_mix == 0) {
				continue;
			}
		} else {
			real_samples_to_mix = (*sample_pos - mdi->info.current_sample);
		}

		mdi->info.current_sample += real_samples_to_mix;
		mdi->sample_count -= real_samples_to_mix;
		if ((mdi->index_count == mdi->index_size) && (mdi->last_note == 0)) {
			mdi->sample_count = 0;
			*sample_pos = mdi->info.current_sample;
			WM_Unlock(&mdi->lock);
			return 0;
		}
	}
	WM_Unlock(&mdi->lock);
#endif
	return 0;
}

int
WildMidi_SampledSeek ( midi * handle, unsigned long int *sample_pos) {
#if 0
	struct _mdi *mdi = (struct _mdi *)handle;
	struct _note **note_data = mdi->note;
	unsigned long int real_samples_to_mix = 0;
	unsigned long int tmp_samples_to_mix = 0;

	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (handle == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
		return -1;
	}
	WM_Lock(&mdi->lock);
	if (sample_pos == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL seek position pointer)", 0);
		WM_Unlock(&mdi->lock);
		return -1;
	}

	if (*sample_pos == mdi->info.current_sample) {
		WM_Unlock(&mdi->lock);
		return 0;
	}

	if (*sample_pos > mdi->info.current_sample) {
		if ((mdi->sample_count == 0) && (mdi->index_count == mdi->index_size) && (mdi->last_note == 0)) {
			*sample_pos = mdi->info.current_sample;
			WM_Unlock(&mdi->lock);
			return 0;
		}
	} else {
		WM_ResetToStart(handle);
		if (note_data != mdi->last_note) {
			do {
				(*note_data)->active = 0;
				*note_data = NULL;
				note_data++;
			} while (note_data != mdi->last_note);
			mdi->last_note = mdi->note;
		}
	}

	while (*sample_pos != mdi->info.current_sample) {
		if (!mdi->sample_count) {
			if (mdi->index_count != mdi->index_size) {

				do {
					if (mdi->index_count == mdi->index_size) {
						break;
					}

					if (mdi->index_count != 0) {
						do_event[((mdi->index[mdi->index_count].event & 0xF0) >> 4) - 8]((mdi->index[mdi->index_count].event & 0x0F), mdi, mdi->index[mdi->index_count].offset);
					}
				} while (mdi->index[mdi->index_count++].delta == 0);

				mdi->samples_to_mix += mdi->index[mdi->index_count-1].delta * mdi->samples_per_delta;
				mdi->sample_count = mdi->samples_to_mix >> 10;
				mdi->samples_to_mix %= 1024;
			} else {
				if (mdi->recalc_samples) {
					WM_RecalcSamples(mdi);
				}
				mdi->sample_count = mdi->info.approx_total_samples - mdi->info.current_sample;
				if (mdi->sample_count == 0) {
					WM_Unlock(&mdi->lock);
					return 0;
				}
			}
		}

		if (mdi->sample_count <= (*sample_pos - mdi->info.current_sample)) {
			real_samples_to_mix = mdi->sample_count;
			if (real_samples_to_mix == 0) {
				continue;
			}
		} else {
			real_samples_to_mix = (*sample_pos - mdi->info.current_sample);
		}

		// do mixing here
		tmp_samples_to_mix = real_samples_to_mix;
		do {

			if (mdi->last_note != mdi->note) {
				note_data = mdi->note;
				while (note_data != mdi->last_note) {


/*
 * ========================
 * sample position checking
 * ========================
 */
					(*note_data)->sample_pos += (*note_data)->sample_inc;
					if (__builtin_expect(((*note_data)->sample_pos > (*note_data)->sample->loop_end), 0)) {
						if ((*note_data)->modes & SAMPLE_LOOP) {
							(*note_data)->sample_pos = (*note_data)->sample->loop_start + (((*note_data)->sample_pos - (*note_data)->sample->loop_start) % (*note_data)->sample->loop_size);
						} else if (__builtin_expect(((*note_data)->sample_pos >= (*note_data)->sample->data_length), 0)) {
							if (__builtin_expect(((*note_data)->next == NULL), 1)) {
								goto KILL_NOTE;
							}
							goto RESTART_NOTE;
						}
					}
					if (__builtin_expect(((*note_data)->env_inc == 0), 0)) {
						note_data++;
						continue;
					}
					(*note_data)->env_level += (*note_data)->env_inc;
					if (__builtin_expect(((*note_data)->env_level > 4194304), 0)) {
						(*note_data)->env_level = (*note_data)->sample->env_target[(*note_data)->env];
					}
					if (__builtin_expect((((*note_data)->env_inc < 0) &&
							((*note_data)->env_level > (*note_data)->sample->env_target[(*note_data)->env])) ||
							(((*note_data)->env_inc > 0) &&
							((*note_data)->env_level < (*note_data)->sample->env_target[(*note_data)->env])), 1)) {
						note_data++;
							continue;
					}
					(*note_data)->env_level = (*note_data)->sample->env_target[(*note_data)->env];
					switch ((*note_data)->env) {
						case 0:
							if (!((*note_data)->modes & SAMPLE_ENVELOPE)) {
								(*note_data)->env_inc = 0;
								note_data++;
								continue;
							}
							break;
						case 2:
							if ((*note_data)->modes & SAMPLE_SUSTAIN) {
							(*note_data)->env_inc = 0;
							note_data++;
							continue;
						}
						break;
					case 5:
						if (__builtin_expect(((*note_data)->env_level == 0), 1)) {
							goto KILL_NOTE;
						}
						// sample release
						if ((*note_data)->modes & SAMPLE_LOOP)
							(*note_data)->modes ^= SAMPLE_LOOP;
						(*note_data)->env_inc = 0;
						note_data++;
						continue;
					case 6:
						if (__builtin_expect(((*note_data)->next != NULL), 1)) {
							RESTART_NOTE:
							(*note_data)->active = 0;
							*note_data = (*note_data)->next;
							(*note_data)->active = 1;
							note_data++;

						} else {
							KILL_NOTE:
							(*note_data)->active = 0;
							mdi->last_note--;
							if (note_data != mdi->last_note) {
								*note_data = *mdi->last_note;
							}
						}
						continue;
					}
					(*note_data)->env++;
					if ((*note_data)->env_level > (*note_data)->sample->env_target[(*note_data)->env]) {
						(*note_data)->env_inc = -(*note_data)->sample->env_rate[(*note_data)->env];
					} else {
					(*note_data)->env_inc = (*note_data)->sample->env_rate[(*note_data)->env];
					}
					note_data++;
					continue;
				}
			} else {
				break;
			}
		} while (--tmp_samples_to_mix);
		mdi->info.current_sample += real_samples_to_mix;
		mdi->sample_count -= real_samples_to_mix;
		if (mdi->index_count == mdi->index_size) {
			if (mdi->last_note == 0) {
				mdi->sample_count = 0;
				*sample_pos = mdi->info.current_sample;
				WM_Unlock(&mdi->lock);
				return 0;
			}
		}
	}
	WM_Unlock(&mdi->lock);
#endif
	return 0;
}

static int
WildMidi_GetOutput_Linear (midi * handle, char * buffer, unsigned long int size) {
	unsigned long int buffer_used = 0;
	unsigned long int i;
	struct _mdi *mdi = (struct _mdi *)handle;
	unsigned long int real_samples_to_mix = 0;
	unsigned long int data_pos;
	signed long int premix, left_mix, right_mix;
	signed long int vol_mul;
	struct _note **note_data = NULL;
	unsigned long int count;
	struct _event *event = mdi->current_event;
	signed long int *tmp_buffer;
	signed long int *out_buffer;

	WM_Lock(&mdi->lock);

	buffer_used = 0;
	memset(buffer, 0, size);
    tmp_buffer = malloc ((size/2) * sizeof(signed long int));
	memset(tmp_buffer, 0, ((size/2) * sizeof(signed long int)));
    out_buffer = tmp_buffer;

	do {
		if (__builtin_expect((!mdi->samples_to_mix),0)) {
            while ((!event->samples_to_next) && (event->do_event != NULL))
            {
                event->do_event(mdi, &event->event_data);
                event++;
            }
            if (event->samples_to_next)
            {
                mdi->current_event = event;
                mdi->samples_to_mix = event->samples_to_next;
                event->samples_to_next = 0;
            } else {
                mdi->samples_to_mix = size >> 2;
            }
		}
		if (__builtin_expect((mdi->samples_to_mix > (size >> 2)),1)) {
			real_samples_to_mix = size >> 2;
		} else {
			real_samples_to_mix = mdi->samples_to_mix;
			if (real_samples_to_mix == 0) {
				continue;
			}
		}

		// do mixing here
		count = real_samples_to_mix;
		do {
			note_data = mdi->note;
			left_mix = right_mix = 0;
			if (__builtin_expect((mdi->last_note != mdi->note),1)) {
				while (note_data != mdi->last_note) {
/*
 * ===================
 * resample the sample
 * ===================
 */
					data_pos = (*note_data)->sample_pos >> FPBITS;
					vol_mul = (((*note_data)->vol_lvl * ((*note_data)->env_level >> 12)) >> FPBITS);

					premix = ((*note_data)->sample->data[data_pos] +
						(((*note_data)->sample->data[data_pos + 1]  - (*note_data)->sample->data[data_pos]) *
						(signed long int)((*note_data)->sample_pos & FPMASK) >> FPBITS)) * vol_mul / 1024;

					left_mix += premix * mdi->channel[(*note_data)->noteid >> 8].left_adjust;
					right_mix += premix * mdi->channel[(*note_data)->noteid >> 8].right_adjust;

/*
 * ========================
 * sample position checking
 * ========================
 */
					(*note_data)->sample_pos += (*note_data)->sample_inc;
					if (__builtin_expect(((*note_data)->sample_pos > (*note_data)->sample->loop_end), 0)) {
						if ((*note_data)->modes & SAMPLE_LOOP) {
							(*note_data)->sample_pos = (*note_data)->sample->loop_start + (((*note_data)->sample_pos - (*note_data)->sample->loop_start) % (*note_data)->sample->loop_size);
						} else if (__builtin_expect(((*note_data)->sample_pos >= (*note_data)->sample->data_length), 0)) {
							if (__builtin_expect(((*note_data)->next == NULL), 1)) {
								goto KILL_NOTE;

							}
							goto RESTART_NOTE;
						}
					}

					if (__builtin_expect(((*note_data)->env_inc == 0), 0)) {
						note_data++;
						continue;
					}

					(*note_data)->env_level += (*note_data)->env_inc;
					if (__builtin_expect(((*note_data)->env_level > 4194304), 0)) {
						(*note_data)->env_level = (*note_data)->sample->env_target[(*note_data)->env];
					}
					if (__builtin_expect((((*note_data)->env_inc < 0) &&
							((*note_data)->env_level > (*note_data)->sample->env_target[(*note_data)->env])) ||
							(((*note_data)->env_inc > 0) &&
							((*note_data)->env_level < (*note_data)->sample->env_target[(*note_data)->env])), 1)) {
						note_data++;
						continue;
					}

					(*note_data)->env_level = (*note_data)->sample->env_target[(*note_data)->env];
					switch ((*note_data)->env) {
						case 0:
							if (!((*note_data)->modes & SAMPLE_ENVELOPE)) {
								(*note_data)->env_inc = 0;
								note_data++;
								continue;
							}
							break;
						case 2:
							if ((*note_data)->modes & SAMPLE_SUSTAIN) {
								(*note_data)->env_inc = 0;
								note_data++;
								continue;
							}
							break;
						case 5:
							if (__builtin_expect(((*note_data)->env_level == 0), 1)) {
								goto KILL_NOTE;
							}
							// sample release
							if ((*note_data)->modes & SAMPLE_LOOP)
								(*note_data)->modes ^= SAMPLE_LOOP;
							(*note_data)->env_inc = 0;
							note_data++;
							continue;
						case 6:
							if (__builtin_expect(((*note_data)->next != NULL), 1)) {
								RESTART_NOTE:
								(*note_data)->active = 0;
								*note_data = (*note_data)->next;
								(*note_data)->active = 1;
								note_data++;

							} else {
								KILL_NOTE:
								(*note_data)->active = 0;
								mdi->last_note--;
								if (note_data != mdi->last_note) {
									*note_data = *mdi->last_note;
								}
							}
							continue;
					}
					(*note_data)->env++;
					if ((*note_data)->env_level > (*note_data)->sample->env_target[(*note_data)->env]) {
						(*note_data)->env_inc = -(*note_data)->sample->env_rate[(*note_data)->env];
					} else {
						(*note_data)->env_inc = (*note_data)->sample->env_rate[(*note_data)->env];
					}
					note_data++;
					continue;
				}
/*
 * =========================
 * mix the channels together
 * =========================
 */

				left_mix /= 1024;
				right_mix /= 1024;
			}

			*tmp_buffer++ = left_mix;
			*tmp_buffer++ = right_mix;

		} while (--count);

		buffer_used += real_samples_to_mix * 4;
		size -= (real_samples_to_mix << 2);
		mdi->info.current_sample += real_samples_to_mix;
		mdi->samples_to_mix -= real_samples_to_mix;
	} while (size);

    tmp_buffer = out_buffer;

	if (mdi->info.mixer_options & WM_MO_REVERB) {
		do_reverb(mdi->reverb, tmp_buffer, (buffer_used / 2));
	}

    for (i = 0; i < buffer_used; i+=4)
    {
        left_mix = *tmp_buffer++;
        right_mix = *tmp_buffer++;

    	if (left_mix > 32767) {
	    	left_mix = 32767;
    	} else if (left_mix < -32768) {
	    	left_mix = -32768;
    	}

    	if (right_mix > 32767) {
    		right_mix = 32767;
    	} else if (right_mix < -32768) {
    		right_mix = -32768;
    	}

/*
 * ===================
 * Write to the buffer
 * ===================
 */
	    (*buffer++) = left_mix & 0xff;
    	(*buffer++) = ((left_mix >> 8) & 0x7f) | ((left_mix >> 24) & 0x80);
	    (*buffer++) = right_mix & 0xff;
		(*buffer++) = ((right_mix >> 8) & 0x7f) | ((right_mix >> 24) &0x80);
    }

    free (out_buffer);
	WM_Unlock(&mdi->lock);
	return buffer_used;
}

static int
WildMidi_GetOutput_Gauss (midi * handle, char * buffer, unsigned long int size) {
	unsigned long int buffer_used = 0;
	unsigned long int i;
	struct _mdi *mdi = (struct _mdi *)handle;
	unsigned long int real_samples_to_mix = 0;
	unsigned long int data_pos;
	signed long int premix, left_mix, right_mix;
	signed long int vol_mul;
	struct _note **note_data = NULL;
	unsigned long int count;
	signed short int *sptr;
	double y, xd;
	double *gptr, *gend;
	int left, right, temp_n;
	int ii, jj;
	struct _event *event = mdi->current_event;
	signed long int *tmp_buffer;
	signed long int *out_buffer;

	WM_Lock(&mdi->lock);

	buffer_used = 0;
	memset(buffer, 0, size);
    tmp_buffer = malloc ((size/2) * sizeof(signed long int));
	memset(tmp_buffer, 0, ((size/2) * sizeof(signed long int)));
    out_buffer = tmp_buffer;

	do {
		if (__builtin_expect((!mdi->samples_to_mix),0)) {
            while ((!event->samples_to_next) && (event->do_event != NULL))
            {
                event->do_event(mdi, &event->event_data);
                event++;
            }
            if (event->samples_to_next)
            {
                mdi->current_event = event;
                mdi->samples_to_mix = event->samples_to_next;
                event->samples_to_next = 0;
            } else {
                mdi->samples_to_mix = size >> 2;
            }
		}
		if (__builtin_expect((mdi->samples_to_mix > (size >> 2)),1)) {
			real_samples_to_mix = size >> 2;
		} else {
			real_samples_to_mix = mdi->samples_to_mix;
			if (real_samples_to_mix == 0) {
				continue;
			}
		}

		// do mixing here
		count = real_samples_to_mix;
		do {
			note_data = mdi->note;
			left_mix = right_mix = 0;
			if (__builtin_expect((mdi->last_note != mdi->note),1)) {
				while (note_data != mdi->last_note) {
/*
 * ===================
 * resample the sample
 * ===================
 */
					data_pos = (*note_data)->sample_pos >> FPBITS;
					vol_mul = (((*note_data)->vol_lvl * ((*note_data)->env_level >> 12)) >> FPBITS);

					/* check to see if we're near one of the ends */
					left = data_pos;
					right = ((*note_data)->sample->data_length>>FPBITS)- left -1;
					temp_n = (right<<1)-1;
					if (temp_n <= 0)
						temp_n = 1;
					if (temp_n > (left<<1)+1)
						temp_n = (left<<1)+1;

					/* use Newton if we can't fill the window */
					if (temp_n < gauss_n) {
						xd = (*note_data)->sample_pos & FPMASK;
						xd /= (1L<<FPBITS);
						xd += temp_n>>1;
						y = 0;
						sptr = (*note_data)->sample->data + ((*note_data)->sample_pos>>FPBITS) - (temp_n>>1);
						for (ii = temp_n; ii;) {
							for (jj = 0; jj <= ii; jj++)
								y += sptr[jj] * newt_coeffs[ii][jj];
							y *= xd - --ii;
						}
						y += *sptr;
					} else {			/* otherwise, use Gauss as usual */
						y = 0;
						gptr = gauss_table[(*note_data)->sample_pos & FPMASK];
						gend = gptr + gauss_n;
						sptr = (*note_data)->sample->data + ((*note_data)->sample_pos >> FPBITS) - (gauss_n>>1);
						do {
							y += *(sptr++) * *(gptr++);
						} while (gptr <= gend);
					}

					premix = y * vol_mul / 1024;

					left_mix += premix * mdi->channel[(*note_data)->noteid >> 8].left_adjust;
					right_mix += premix * mdi->channel[(*note_data)->noteid >> 8].right_adjust;

/*
 * ========================
 * sample position checking
 * ========================
 */
					(*note_data)->sample_pos += (*note_data)->sample_inc;
					if (__builtin_expect(((*note_data)->sample_pos > (*note_data)->sample->loop_end), 0)) {
						if ((*note_data)->modes & SAMPLE_LOOP) {
							(*note_data)->sample_pos = (*note_data)->sample->loop_start + (((*note_data)->sample_pos - (*note_data)->sample->loop_start) % (*note_data)->sample->loop_size);
						} else if (__builtin_expect(((*note_data)->sample_pos >= (*note_data)->sample->data_length), 0)) {
							if (__builtin_expect(((*note_data)->next == NULL), 1)) {
								goto KILL_NOTE;

							}
							goto RESTART_NOTE;
						}
					}

					if (__builtin_expect(((*note_data)->env_inc == 0), 0)) {
						note_data++;
						continue;
					}

					(*note_data)->env_level += (*note_data)->env_inc;
					if (__builtin_expect(((*note_data)->env_level > 4194304), 0)) {
						(*note_data)->env_level = (*note_data)->sample->env_target[(*note_data)->env];
					}
					if (__builtin_expect((((*note_data)->env_inc < 0) &&
							((*note_data)->env_level > (*note_data)->sample->env_target[(*note_data)->env])) ||
							(((*note_data)->env_inc > 0) &&
							((*note_data)->env_level < (*note_data)->sample->env_target[(*note_data)->env])), 1)) {
						note_data++;
						continue;
					}

					(*note_data)->env_level = (*note_data)->sample->env_target[(*note_data)->env];
					switch ((*note_data)->env) {
						case 0:
							if (!((*note_data)->modes & SAMPLE_ENVELOPE)) {
								(*note_data)->env_inc = 0;
								note_data++;
								continue;
							}
							break;
						case 2:
							if ((*note_data)->modes & SAMPLE_SUSTAIN) {
								(*note_data)->env_inc = 0;
								note_data++;
								continue;
							}
							break;
						case 5:
							if (__builtin_expect(((*note_data)->env_level == 0), 1)) {
								goto KILL_NOTE;
							}
							// sample release
							if ((*note_data)->modes & SAMPLE_LOOP)
								(*note_data)->modes ^= SAMPLE_LOOP;
							(*note_data)->env_inc = 0;
							note_data++;
							continue;
						case 6:
							if (__builtin_expect(((*note_data)->next != NULL), 1)) {
								RESTART_NOTE:
								(*note_data)->active = 0;
								*note_data = (*note_data)->next;
								(*note_data)->active = 1;
								note_data++;

							} else {
								KILL_NOTE:
								(*note_data)->active = 0;
								mdi->last_note--;
								if (note_data != mdi->last_note) {
									*note_data = *mdi->last_note;
								}
							}
							continue;
					}
					(*note_data)->env++;
					if ((*note_data)->env_level > (*note_data)->sample->env_target[(*note_data)->env]) {
						(*note_data)->env_inc = -(*note_data)->sample->env_rate[(*note_data)->env];
					} else {
						(*note_data)->env_inc = (*note_data)->sample->env_rate[(*note_data)->env];
					}
					note_data++;
					continue;
				}
/*
 * =========================
 * mix the channels together
 * =========================
 */

				left_mix /= 1024;
				right_mix /= 1024;
			}

			*tmp_buffer++ = left_mix;
			*tmp_buffer++ = right_mix;

		} while (--count);

		buffer_used += real_samples_to_mix * 4;
		size -= (real_samples_to_mix << 2);
		mdi->info.current_sample += real_samples_to_mix;
		mdi->samples_to_mix -= real_samples_to_mix;
	} while (size);

    tmp_buffer = out_buffer;

	if (mdi->info.mixer_options & WM_MO_REVERB) {
		do_reverb(mdi->reverb, tmp_buffer, (buffer_used / 2));
	}

    for (i = 0; i < buffer_used; i+=4)
    {
        left_mix = *tmp_buffer++;
        right_mix = *tmp_buffer++;

    	if (left_mix > 32767) {
	    	left_mix = 32767;
    	} else if (left_mix < -32768) {
	    	left_mix = -32768;
    	}

    	if (right_mix > 32767) {
    		right_mix = 32767;
    	} else if (right_mix < -32768) {
    		right_mix = -32768;
    	}

/*
 * ===================
 * Write to the buffer
 * ===================
 */
	    (*buffer++) = left_mix & 0xff;
    	(*buffer++) = ((left_mix >> 8) & 0x7f) | ((left_mix >> 24) & 0x80);
	    (*buffer++) = right_mix & 0xff;
		(*buffer++) = ((right_mix >> 8) & 0x7f) | ((right_mix >> 24) &0x80);
    }
    free (out_buffer);
	WM_Unlock(&mdi->lock);
	return buffer_used;
}

int
WildMidi_GetOutput (midi * handle, char * buffer, unsigned long int size) {
	struct _mdi *mdi = (struct _mdi *)handle;

	if (__builtin_expect((!WM_Initialized),0)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (__builtin_expect((handle == NULL),0)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
		return -1;
	}
	if (__builtin_expect((buffer == NULL),0)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL buffer pointer)", 0);
		return -1;
	}

	if (__builtin_expect((size == 0),0)) {
		return 0;
	}

	if (__builtin_expect((size % 4),0)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(size not a multiple of 4)", 0);
		return -1;
	}
	if (mdi->info.mixer_options & WM_MO_ENHANCED_RESAMPLING) {
		return WildMidi_GetOutput_Gauss (handle, buffer,size);
	} else {
		return WildMidi_GetOutput_Linear (handle, buffer, size);
	}
}

int
WildMidi_SetOption (midi * handle, unsigned short int options, unsigned short int setting) {
	struct _mdi *mdi = (struct _mdi *)handle;
	struct _note **note_data = mdi->note;
	int i;

	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (handle == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
		return -1;
	}
	WM_Lock(&mdi->lock);
	if ((!(options & 0x0007)) || (options & 0xFFF8)){
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid option)", 0);
		WM_Unlock(&mdi->lock);
		return -1;
	}
	if (setting & 0xFFF8) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid setting)", 0);
		WM_Unlock(&mdi->lock);
		return -1;
	}

	mdi->info.mixer_options = ((mdi->info.mixer_options & (0x00FF ^ options)) | (options & setting));

	if (options & WM_MO_LOG_VOLUME) {
    	if (mdi->info.mixer_options & WM_MO_LOG_VOLUME) {
    		mdi->amp = (281 * ((mdi->lin_max_vol << 10) / mdi->log_max_vol)) >> 10;
		} else {
			mdi->amp = 281;
		}

   	    mdi->amp = (mdi->amp * (((lin_volume[127] * lin_volume[127]) << 10) / mdi->lin_max_vol)) >> 10;

		for (i = 0; i < 16; i++) {
			do_pan_adjust(mdi, i);
		}

		if (note_data != mdi->last_note) {
			do {
				(*note_data)->vol_lvl = get_volume(mdi, ((*note_data)->noteid >> 8), *note_data);
				if ((*note_data)->next)
					(*note_data)->next->vol_lvl = get_volume(mdi, ((*note_data)->noteid >> 8), (*note_data)->next);
				note_data++;
			} while (note_data != mdi->last_note);
		}
	} else if (options & WM_MO_REVERB) {
		reset_reverb(mdi->reverb);
	}

	WM_Unlock(&mdi->lock);
	return 0;
}

struct _WM_Info *
WildMidi_GetInfo (midi * handle) {
	struct _mdi *mdi = (struct _mdi *)handle;
	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return NULL;
	}
	if (handle == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
		return NULL;
	}
	WM_Lock(&mdi->lock);
	if (mdi->tmp_info == NULL) {
		mdi->tmp_info = malloc(sizeof(struct _WM_Info));
		if (mdi->tmp_info == NULL) {
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to set info", 0);
			WM_Unlock(&mdi->lock);
			return NULL;
		}
	}
	mdi->tmp_info->current_sample = mdi->info.current_sample;
	mdi->tmp_info->approx_total_samples = mdi->info.approx_total_samples;
	mdi->tmp_info->mixer_options = mdi->info.mixer_options;
	WM_Unlock(&mdi->lock);
	return mdi->tmp_info;
}

int
WildMidi_Shutdown ( void ) {
	struct _hndl * tmp_hdle;

	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (first_handle != NULL) {
		while (first_handle != NULL) {
			tmp_hdle = first_handle->next;
			WildMidi_Close((struct _mdi *)first_handle->handle);
			free (first_handle);
			first_handle = tmp_hdle;
		}
	}
	WM_FreePatches();
	free_gauss();
	WM_Initialized = 0;
	return 0;
}

