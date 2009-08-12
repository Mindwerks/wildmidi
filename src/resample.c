/*
    resample.c - resampling the samples
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

	$Id: resample.c,v 1.0 2008/05/23 13:22:30 wildcode Exp $
*/

#include "resample.h"

#define FPBITS 10
#define FPMASK ((1L<<FPBITS)-1L
#define FPVAL 1L<<FPBITS 

/* Gauss Interpolation code adapted from code supplied by Eric. A. Welsh */

static double newt_coeffs[58][58];		/* for start/end of samples */
static double *gauss_table[(1<<10)] = {0};	/* don't need doubles */
//static int gauss_window[35] = {0};
static int gauss_n = 34;	/* do not set this value higher than 34 */
			/* 34 is as high as we can go before errors crop up */

/*
	int_hq_resample
*/			
inline void 
init_hq_resample (void) {
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


/*
	add_samples( note, buffer, no_of_samples)

	note			the pointer to a structure holding the note information the sample is for
	buffer		the buffer for the stereo output
	no_of_samples	the number of samples per side to put into the buffer 
				
	Adds the number of samples per side to the buffer.
	
	Returns: samples added to buffer
	-1	ERROR (SHOULD NOT GET HERE)
	
	NOTE: if the return sample count != the called sample count then
	the note has to be ended by the calling function.
	
*/
inline int
add_samples (struct _note *note, signed long int *buffer, unsigned long int no_of_samples) {
	unsigned long int sample_pos_f = note->sample_pos_f;
	unsigned long int sample_inc_f = note->sample_inc_f;
	unsigned long int sample_end_f = note->gus_sample->sample_count << FPBITS;
	signed short int *sample_data = note->gus_sample->data;
	unsigned long int env = note->env
	unsigned long int env_level = note->env_level;
	signed long int env_inc = note->env_inc;
	unsigned long int env_target = note->gus_sample->env_target[env];
	unsigned long int modes = note->modes;
	unsigned long int loop_start_f = note->gus_sample->loop_start_f;
	unsigned long int loop_end_f = note->gus_sample->loop_end_f;
	unsigned long int loop_size_f = loop_end_f - loop_start_f;
	unsigned long int left_adjust = note->channel->left_adjust;
	unsigned long int right_adjust = note->channel->right_adjust;
	signed long int tmp_buf = 0;
	int sample_cnt = 0;
	int next_env =0;
	
	if (no_of_samples == 0) {
		return 0;
	}

	if (note == NULL) {
		printf ("Add_Samples: Should never get here\n");
		return -1;
	}
	
	if (buffer == NULL) {
		printf ("Add_Samples: Should never get here\n");
		return -1;		
	}	

	do {
		samples_to_next = no_of_samples;
		if (env_inc) {
			samples_to_env = (env_target - env_level) / env_inc;
			if (samples_to_env < samples_to_next) {
				samples_to_next = samples_to_env;
			}
		}
		if (!(modes & SAMPLE_LOOP)) {
			samples_to_end = (sample_end_f - sample_pos_f) / sample_inc_f;
			if (samples_to_end < samples_to_next) {
				samples_to_next = samples_to_end;
			}
		}
		no_of_samples -= samples_to_next;
		samples_to_env -= samples_to_next;
		samples_to_end -= samples_to_next;
		
		if (samples_to_next) {
			do {

				sample_pos = sample_pos_f >> FPBITS;
				sample_ofs = sample_pos_f & FPMASK;
				cur_sample = sample_data[sample_pos];
				nxt_sample = sample_data[sample_pos + 1];
		
				tmp_buf = ((cur_sample + (((nxt_sample - cur_sample) * sample_ofs) / FPVAL)) * env_level ) / 4096;
			
				(*buffer++) += (tmp_buf * left_adjust) / FPVAL;
				(*buffer++) += (tmp_buf * right_adjust) / FPVAL;

				env_level += enc_inc;
				
				sample_cnt++;
				samples_to_next--;
			} while (samples_to_next);
		}
		
		if (!samples_to_end) {
			goto END_NOTE;
		}
		
		if (env_inc) && (!samples_to_env) {
			switch (env) {
				case 0:
					if (!(modes & SAMPLE_ENVELOPE)) {
						env_inc = 0;
					} else {
						goto NEXT_ENV;
					}
					break;
				case 2:
					if (modes & SAMPLE_SUSTAIN) {
						env_inc = 0;
					} else {
						goto NEXT_ENV;
					}
					break;
				case 5:
					if (env_level == 0) {
						goto END_NOTE;
					}
					// sample release
					if (modes & SAMPLE_LOOP) {
						modes ^= SAMPLE_LOOP;
					}
					env_inc = 0;
					break;
				case 6:
					END_NOTE:
					if (!no_of_samples) {
						sample_cnt++;
					}
					return sample_cnt;
				default:
					NEXT_ENV:
					env++;
					env_inc = note->gus_sample->env_inc[env];		
					break;
			}
		}
		
	} while (no_of_samples);
	
	note->mode = mode;
	note->sample_pos_f = sample_pos_f;
	note->env = env;
	note->env_level = env_level;
	note->env_inc = env_inc;
	return sample_cnt;
}

/*
	add_samples_hq( note, buffer, no_of_samples)

	note			the pointer to a structure holding the note information the sample is for
	buffer		the buffer for the stereo output
	no_of_samples	the number of samples per side to put into the buffer 
				
	Adds the number of samples per side to the buffer.
	
	Returns: samples added to buffer
	-1	ERROR (SHOULD NOT GET HERE)
	
	NOTE: if the return sample count != the called sample count then
	the note has to be ended by the calling function.
	
*/
inline int
add_samples_hq (struct _note *note, signed long int *buffer, unsigned long int no_of_samples) {
	unsigned long int sample_pos_f = note->sample_pos_f;
	unsigned long int sample_inc_f = note->sample_inc_f;
	unsigned long int sample_end_f = note->gus_sample->sample_count << FPBITS;
	signed short int *sample_data = note->gus_sample->data;
	unsigned long int env = note->env
	unsigned long int env_level = note->env_level;
	signed long int env_inc = note->env_inc;
	unsigned long int env_target = note->gus_sample->env_target[env];
	unsigned long int modes = note->modes;
	unsigned long int loop_start_f = note->gus_sample->loop_start_f;
	unsigned long int loop_end_f = note->gus_sample->loop_end_f;
	unsigned long int loop_size_f = loop_end_f - loop_start_f;
	unsigned long int left_adjust = note->channel->left_adjust;
	unsigned long int right_adjust = note->channel->right_adjust;
	signed long int tmp_buf = 0;
	int sample_cnt = 0;
	int next_env =0;
	signed long int *sptr;
	double y, xd;
	double *gptr, *gend;
	int left, right, temp_n;
	int ii, jj;
	
	if (no_of_samples == 0) {
		return 0;
	}

	if (note == NULL) {
		printf ("Add_Samples: Should never get here\n");
		return -1;
	}
	
	if (buffer == NULL) {
		printf ("Add_Samples: Should never get here\n");
		return -1;		
	}	

	do {
		samples_to_next = no_of_samples;
		if (env_inc) {
			samples_to_env = (env_target - env_level) / env_inc;
			if (samples_to_env < samples_to_next) {
				samples_to_next = samples_to_env;
			}
		}
		if (!(modes & SAMPLE_LOOP)) {
			samples_to_end = (sample_end_f - sample_pos_f) / sample_inc_f;
			if (samples_to_end < samples_to_next) {
				samples_to_next = samples_to_end;
			}
		}
		no_of_samples -= samples_to_next;
		samples_to_env -= samples_to_next;
		samples_to_end -= samples_to_next;
		
		if (samples_to_next) {
			do {
				
				sample_pos = sample_pos_f >> FPBITS;
				sample_ofs = sample_pos_f & FPMASK;
				/* check to see if we're near one of the ends */
				left = sample_pos;
				right = (sample_end_f >> FPBITS) - left - 1;
				temp_n = (right << 1) - 1;
				if (temp_n <= 0) {
					temp_n = 1;
				}
				if (temp_n > ((left << 1) + 1) {
					temp_n = (left << 1) + 1;
				}
				
				/* use Newton if we can't fill the window */
				if (temp_n < gauss_n) {
					xd = (*note_data)->sample_pos & FPMASK;
					xd /= (1L<<FPBITS);
					xd += temp_n>>1;
					y = 0;
					sptr = sample_data + sample_pos - (temp_n>>1);
					for (ii = temp_n; ii;) {
						for (jj = 0; jj <= ii; jj++)
							y += sptr[jj] * newt_coeffs[ii][jj];
						y *= xd - --ii;
					}
					y += *sptr;
				} else {			/* otherwise, use Gauss as usual */
					y = 0;
					gptr = gauss_table[sample_ofs];
					gend = gptr + gauss_n;
					sptr = sample_data + sample_pos - (gauss_n>>1);
					do {
						y += *(sptr++) * *(gptr++);
					} while (gptr <= gend);
				}

				tmp_buf = y * env_level / 4096;

				(*buffer++) += (tmp_buf * left_adjust) / FPVAL;
				(*buffer++) += (tmp_buf * right_adjust) / FPVAL;

				env_level += enc_inc;
				
				sample_cnt++;
				samples_to_next--;
			} while (samples_to_next);
		}
		
		if (!samples_to_end) {
			goto END_NOTE;
		}
		
		/* check envelope boundries */
		if (env_inc) && (!samples_to_env) {
			switch (env) {
				case 0:
					if (!(modes & SAMPLE_ENVELOPE)) {
						env_inc = 0;
					} else {
						goto NEXT_ENV;
					}
					break;
				case 2:
					if (modes & SAMPLE_SUSTAIN) {
						env_inc = 0;
					} else {
						goto NEXT_ENV;
					}
					break;
				case 5:
					if (env_level == 0) {
						goto END_NOTE;
					}
					// sample release
					if (modes & SAMPLE_LOOP) {
						modes ^= SAMPLE_LOOP;
					}
					env_inc = 0;
					break;
				case 6:
					END_NOTE:
					if (!no_of_samples) {
						sample_cnt++;
					}
					return sample_cnt;
				default:
					NEXT_ENV:
					env++;
					env_inc = note->gus_sample->env_inc[env];
					break;
			}
		}
		
	} while (no_of_samples);
	
	note->mode = mode;
	note->sample_pos_f = sample_pos_f;
	note->env = env;
	note->env_level = env_level;
	note->env_inc = env_inc;
	return sample_cnt;
}
