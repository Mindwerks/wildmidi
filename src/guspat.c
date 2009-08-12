/*
    guspat.c - process gus patch files
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

    $Id: guspat.c,v 1.9 2008/05/23 13:22:31 wildcode Exp $
*/

#include <errno.h>
#include <stdlib.h>
#include <string.h>


#include "error.h"
#include "file_io.h"
#include "guspat.h"

/*
	free_sample - Free the memory used by the samples
	
	void free_sample (struct _sample *sample) 
	
	sample	address of the start of that sample chain
	
	Returns Nothing
*/

inline void
free_sample (struct _sample *sample) {
	struct _sample *temp_sample = NULL;

	if (sample != NULL) {
		/*
			Free each sample in the chain
		*/
		do {
			temp_sample = sample->next;
			free(sample);
			sample = temp_sample;
		} while (sample != NULL);
	}
	
	/*
		Done
	*/
	return;
}

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

/*
	load_guspat - Load Gravis Ultrasound Patch File
	
	struct _sample *load_guspat (const char *guspat_file)

	guspat_file		the filename of the Gravis Ultrasound patch file
	
	Returns:
		Address where samples are stored
		NULL		Error
		
	NOTE: Calling function will need to free the data once no-longer needed

	==============================================================
	
	GUSPAT File Format (well the bits we use anyways)
	
	Ofs		Bytes		Description
	0x0		12		Contains GF1PATCH110\0 or GF1PATCH100\0
	0xC		10		Contains ID#000002\0
	0x52		1		Number of instruments, we support only 1 so expect 1
	0x97		1		Number of layers, we supportonly 1 so expect 1
	0xC6		1		Number of Samples, must be atleast 1
	0xEF				Start of Sample Data
	
	Sample Data offsets
	Ofs		Bytes		Description	
	0x7		1		Loop fractions
	0x8		4		Sample size
	0xC		4		Loop start
	0x10		4		Loop end
	0x14		2		Sample Rate
	0x16		4		Sample frequency high
	0x1A		4		Sample frequency low
	0x1E		4		Sample root frequency
	0x25		6		Envelope Rates
	0x2B		6		Envelope Level
	0x37		1		Sample Modes
	0x60				Sample Data
	
*/

#define SAMPLE_16BIT 0x01
#define SAMPLE_UNSIGNED 0x02
#define SAMPLE_LOOP 0x04
#define SAMPLE_PINGPONG 0x08
#define SAMPLE_REVERSE 0x10
#define SAMPLE_SUSTAIN 0x20
#define SAMPLE_ENVELOPE 0x40

inline struct _sample *
load_guspat (const char *guspat_file) {	
	struct _sample *ret_samples = NULL;
	struct _sample *temp_sample = NULL;
	unsigned char *file_data = NULL;
	unsigned long int file_size = 0;
	unsigned long int file_ofs = 0;
	unsigned char env_cnt = 0;
	unsigned char no_of_samples = 0;
	float env_time_table[] = {
		0.0, 0.092857143, 0.046428571, 0.030952381, 0.023214286, 0.018571429, 0.015476190, 0.013265306, 0.011607143, 0.010317460, 0.009285714, 0.008441558, 0.007738095, 0.007142857, 0.006632653, 0.006190476, 0.005803571, 0.005462185, 0.005158730, 0.004887218, 0.004642857, 0.004421769, 0.004220779, 0.004037267, 0.003869048, 0.003714286, 0.003571429, 0.003439153, 0.003316327, 0.003201970, 0.003095238, 0.002995392, 0.002901786, 0.002813853, 0.002731092, 0.002653061, 0.002579365, 0.002509653, 0.002443609, 0.002380952, 0.002321429, 0.002264808, 0.002210884, 0.002159468, 0.002110390, 0.002063492, 0.002018634, 0.001975684, 0.001934524, 0.001895044, 0.001857143, 0.001820728, 0.001785714, 0.001752022, 0.001719577, 0.001688312, 0.001658163, 0.001629073, 0.001600985, 0.001573850, 0.001547619, 0.001522248, 0.001497696, 0.001473923,
		0.0, 0.742857143, 0.371428571, 0.247619048, 0.185714286, 0.148571429, 0.123809524, 0.106122449, 0.092857143, 0.082539683, 0.074285714, 0.067532468, 0.061904762, 0.057142857, 0.053061224, 0.049523810, 0.046428571, 0.043697479, 0.041269841, 0.039097744, 0.037142857, 0.035374150, 0.033766234, 0.032298137, 0.030952381, 0.029714286, 0.028571429, 0.027513228, 0.026530612, 0.025615764, 0.024761905, 0.023963134, 0.023214286, 0.022510823, 0.021848739, 0.021224490, 0.020634921, 0.020077220, 0.019548872, 0.019047619, 0.018571429, 0.018118467, 0.017687075, 0.017275748, 0.016883117, 0.016507937, 0.016149068, 0.015805471, 0.015476190, 0.015160350, 0.014857143, 0.014565826, 0.014285714, 0.014016173, 0.013756614, 0.013506494, 0.013265306, 0.013032581, 0.012807882, 0.012590799, 0.012380952, 0.012177986, 0.011981567, 0.011791383,
		0.0, 5.942857143, 2.971428571, 1.980952381, 1.485714286, 1.188571429, 0.990476190, 0.848979592, 0.742857143, 0.660317460, 0.594285714, 0.540259740, 0.495238095, 0.457142857, 0.424489796, 0.396190476, 0.371428571, 0.349579832, 0.330158730, 0.312781955, 0.297142857, 0.282993197, 0.270129870, 0.258385093, 0.247619048, 0.237714286, 0.228571429, 0.220105820, 0.212244898, 0.204926108, 0.198095238, 0.191705069, 0.185714286, 0.180086580, 0.174789916, 0.169795918, 0.165079365, 0.160617761, 0.156390977, 0.152380952, 0.148571429, 0.144947735, 0.141496599, 0.138205980, 0.135064935, 0.132063492, 0.129192547, 0.126443769, 0.123809524, 0.121282799, 0.118857143, 0.116526611, 0.114285714, 0.112129380, 0.110052910, 0.108051948, 0.106122449, 0.104260652, 0.102463054, 0.100726392, 0.099047619, 0.097423888, 0.095852535, 0.094331066,
		0.0, 47.542857143, 23.771428571, 15.847619048, 11.885714286, 9.508571429, 7.923809524, 6.791836735, 5.942857143, 5.282539683, 4.754285714, 4.322077922, 3.961904762, 3.657142857, 3.395918367, 3.169523810, 2.971428571, 2.796638655, 2.641269841, 2.502255639, 2.377142857, 2.263945578, 2.161038961, 2.067080745, 1.980952381, 1.901714286, 1.828571429, 1.760846561, 1.697959184, 1.639408867, 1.584761905, 1.533640553, 1.485714286, 1.440692641, 1.398319328, 1.358367347, 1.320634921, 1.284942085, 1.251127820, 1.219047619, 1.188571429, 1.159581882, 1.131972789, 1.105647841, 1.080519481, 1.056507937, 1.033540373, 1.011550152, 0.990476190, 0.970262391, 0.950857143, 0.932212885, 0.914285714, 0.897035040, 0.880423280, 0.864415584, 0.848979592, 0.834085213, 0.819704433, 0.805811138, 0.792380952, 0.779391101, 0.766820276, 0.754648526
	};	
	
	if (guspat_file == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,"No Filename!", 0);
		return NULL;
	}
	
	/*
		Load the Gravis Ultrasound patch file into memory
	*/
	if ((file_data = WM_BufferFile(guspat_file, &file_size)) == NULL) {
		/*
			An error occured loading the file
		*/
        return NULL;
	}
	
	if (file_size < 239) {
		/*
			should be bigger than this
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, guspat_file, 0);
		free(file_data);
		return NULL;
	}
	
	if (memcmp(file_data, "GF1PATCH110\0ID#000002\0", 22) && memcmp(file_data, "GF1PATCH100\0ID#000002\0", 22)) {
		/*
			This is not a supported GUSPAT file
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID,"(unsupported format)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, guspat_file, 0);
		free(file_data);
		return NULL;
	}
	
	/*
		Make sure we only have 1 instrument
	*/
	if (file_data[82] > 1) {
		/*
			This is not a supported GUSPAT file
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID,"(unsupported format)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, guspat_file, 0);
		free(file_data);
		return NULL;
	}
	
	/*
		Make sure we only have 1 layer
	*/
	if (file_data[151] > 1) {
		/*
			This is not a supported GUSPAT file
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID,"(unsupported format)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, guspat_file, 0);
		free(file_data);
		return NULL;
	}
					
	/*
		Make sure we have alteast 1 sample
	*/
	no_of_samples = file_data[198];
	if (no_of_samples == 0) {
		/*
			No samples in GUSPAT
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID,"(no samples)", 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, guspat_file, 0);
		free(file_data);
		return NULL;
	}
	
	file_ofs = 239;
	if ((ret_samples = malloc(sizeof(struct _sample))) == NULL) {
		/*
			Error getting memory
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, 0);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, guspat_file, 0);
		free(file_data);
		return NULL;
	}
	
	/*
		grab the sample
	*/
	while (no_of_samples--) {
		if (temp_sample != NULL) {
			if ((temp_sample->next = malloc(sizeof(struct _sample))) == NULL) {
				/*
					Error getting memory
				*/
				WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, 0);
				WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, guspat_file, 0);
				free_sample(ret_samples);
				free(file_data);
				return NULL;
			}
			temp_sample = temp_sample->next;
		} else {
			temp_sample = ret_samples;
		}
		
		/*
			get sample information from file
		*/
		temp_sample->next = NULL;
		temp_sample->loop_fraction = file_data[file_ofs+7];
		temp_sample->count = (file_data[file_ofs+11] << 24) | (file_data[file_ofs+10] << 16) | (file_data[file_ofs+9] << 8) | file_data[file_ofs+8];
		temp_sample->loop_start = (file_data[file_ofs+15] << 24) | (file_data[file_ofs+14] << 16) | (file_data[file_ofs+13] << 8) | file_data[file_ofs+12];
		temp_sample->loop_end = (file_data[file_ofs+19] << 24) | (file_data[file_ofs+18] << 16) | (file_data[file_ofs+17] << 8) | file_data[file_ofs+16];
		temp_sample->rate = (file_data[file_ofs+21] << 8) | file_data[file_ofs+20];
		temp_sample->freq_low = ((file_data[file_ofs+25] << 24) | (file_data[file_ofs+24] << 16) | (file_data[file_ofs+23] << 8) | file_data[file_ofs+22]);
		temp_sample->freq_high = ((file_data[file_ofs+29] << 24) | (file_data[file_ofs+28] << 16) | (file_data[file_ofs+27] << 8) | file_data[file_ofs+26]);
		temp_sample->freq_root = ((file_data[file_ofs+33] << 24) | (file_data[file_ofs+32] << 16) | (file_data[file_ofs+31] << 8) | file_data[file_ofs+30]);

		temp_sample->modes = file_data[file_ofs+55] & 0x7F;

		for (env_cnt = 0; env_cnt < 6; env_cnt++) {
			if (temp_sample->modes & SAMPLE_ENVELOPE) {
				temp_sample->env_rate[env_cnt] = (unsigned long int)(4194303.0 / env_time_table[file_data[file_ofs+37+env_cnt]]);
				temp_sample->env_target[env_cnt] = 16448 * file_data[file_ofs+43+env_cnt];
			} else {
				temp_sample->env_rate[env_cnt]  = (unsigned long int)(4194303.0 /  env_time_table[63]);
				temp_sample->env_target[env_cnt] = 4194303;
			}
		}

		/*
			this is a super fast release envelope added so that killing of notes does not create clicking.
		*/
		temp_sample->env_target[6] = 0;
		temp_sample->env_rate[6]  = (unsigned long int)(4194303.0 / env_time_table[63]);

		/*
			copy the sample
		*/
		if ((temp_sample->samples = malloc(temp_sample->count)) == NULL) {
			/*
				Error getting memory
			*/
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, 0);
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, guspat_file, 0);
			free_sample(ret_samples);
			free(file_data);
			return NULL;
		}
		
		memcpy(temp_sample->samples, &file_data[file_ofs+96], temp_sample->count);
		
		file_ofs += 96 + temp_sample->count;
		
		/* 
			if 8bit, convert to 16bit 
		*/
		if ((temp_sample->modes & SAMPLE_16BIT)) {
			temp_sample->count >>= 1;
			temp_sample->loop_start >>= 1;
			temp_sample->loop_end >>= 1;
		} else {
			if ((temp_sample->samples = convert_8to16((unsigned char *)temp_sample->samples, temp_sample->count)) == NULL) {
				free_sample(ret_samples);
				free(file_data);
				return NULL;
			}
		}
		
		/* 
			if unsigned, convert to signed
		*/
		if ((temp_sample->modes & SAMPLE_UNSIGNED)) {
			convert_sign(temp_sample->samples, temp_sample->count);
		}

		/* 
			switch around if reversed
		*/
		if ((temp_sample->modes & SAMPLE_REVERSE)) {
			unsigned long int temp_loop_start = 0;
			if ((temp_sample->samples = convert_reverse(temp_sample->samples, temp_sample->count)) == NULL) {
				free_sample(ret_samples);
				free(file_data);
				return NULL;
			}

			temp_loop_start = temp_sample->loop_start;
			temp_sample->loop_start = temp_sample->loop_end;
			temp_sample->loop_end = temp_loop_start;
			temp_sample->loop_fraction = ((temp_sample->loop_fraction & 0x0f) << 4) | ((temp_sample->loop_fraction & 0xf0) >> 4);
		}
		
		/* 
			if pingpong then straighten it out a bit
		*/
		if ((temp_sample->modes & SAMPLE_PINGPONG)) {
			unsigned long int tmp_loop_size = 0;
			if ((temp_sample->samples = convert_pingpong(temp_sample->samples, temp_sample->count, temp_sample->loop_start, temp_sample->loop_end)) == NULL) {
				free_sample(ret_samples);
				free(file_data);
				return NULL;
			}
			tmp_loop_size = (temp_sample->loop_end - temp_sample->loop_start) << 1;
			temp_sample->loop_end = temp_sample->loop_start + (tmp_loop_size);
			temp_sample->count += tmp_loop_size;
		}
	}
	
	free(file_data);

	/*
		Return the memory location where the sample data was stored
	*/
	return ret_samples;
}
