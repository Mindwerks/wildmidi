/*
 file_io.c

 file handling

 Copyright (C) Chris Ison  2001-2011
 Copyright (C) Bret Curtis 2013-2014

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
 */

#include "config.h"

#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#undef close
#define close _close
#undef open
#define open _open
#undef read
#define read _read
#elif defined(__DJGPP__)
#include <io.h>
#include <dir.h>
#include <unistd.h>
#else
#include <pwd.h>
#include <strings.h>
#include <unistd.h>
#endif

#if !defined(O_BINARY)
# if defined(_O_BINARY)
#  define O_BINARY _O_BINARY
# else
#  define O_BINARY  0
# endif
#endif

#include "wm_error.h"
#include "file_io.h"

void *_WM_BufferFile(const char *filename, uint32_t *size) {
	int buffer_fd;
	uint8_t *data;
#ifdef __DJGPP__
	struct ffblk f;
#else
	struct stat buffer_stat;
#endif
#if !defined(_WIN32) && !defined(__DJGPP__)
	const char *home = NULL;
	struct passwd *pwd_ent;
	char buffer_dir[1024];
#endif /* unix builds */
	char *buffer_file = NULL;

#if !defined(_WIN32) && !defined(__DJGPP__)
	if (strncmp(filename, "~/", 2) == 0) {
		if ((pwd_ent = getpwuid(getuid()))) {
			home = pwd_ent->pw_dir;
		} else {
			home = getenv("HOME");
		}
		if (home) {
			buffer_file = malloc(strlen(filename) + strlen(home) + 1);
			if (buffer_file == NULL) {
				_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
				_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, errno);
				return NULL;
			}
			strcpy(buffer_file, home);
			strcat(buffer_file, filename + 1);
		}
	} else if (filename[0] != '/') {
		char* cwdresult = getcwd(buffer_dir, 1024);
		if (cwdresult != NULL)
			buffer_file = malloc(strlen(filename) + strlen(buffer_dir) + 2);
		if (buffer_file == NULL || cwdresult == NULL) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, errno);
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
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, errno);
			return NULL;
		}
		strcpy(buffer_file, filename);
	}

#ifdef __DJGPP__
	if (findfirst(buffer_file, &f, FA_ARCH | FA_RDONLY) != 0) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_STAT, filename, errno);
		free(buffer_file);
		return NULL;
	}
	*size = f.ff_fsize;
#else
	if (stat(buffer_file, &buffer_stat)) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_STAT, filename, errno);
		free(buffer_file);
		return NULL;
	}
	*size = buffer_stat.st_size;
#endif

	if (__builtin_expect((*size > WM_MAXFILESIZE), 0)) {
		/* don't bother loading suspiciously long files */
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LONGFIL, filename, 0);
		free(buffer_file);
		return NULL;
	}

	/* +1 needed for parsing text files without a newline at the end */
	data = (uint8_t *) malloc(*size + 1);
	if (data == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, errno);
		free(buffer_file);
		return NULL;
	}

	if ((buffer_fd = open(buffer_file,(O_RDONLY | O_BINARY))) == -1) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_OPEN, filename, errno);
		free(buffer_file);
		free(data);
		return NULL;
	}
	if (read(buffer_fd, data, *size) != (long) *size) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_READ, filename, errno);
		free(buffer_file);
		free(data);
		close(buffer_fd);
		return NULL;
	}
	data[*size] = '\0';

	close(buffer_fd);
	free(buffer_file);
	return data;
}

