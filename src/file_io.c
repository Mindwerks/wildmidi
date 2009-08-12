/*
    file_io.c Process files
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
 
     $Id: file_io.c,v 1.15 2008/06/04 13:08:27 wildcode Exp $
*/

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef _WIN32
# include <pwd.h>
#endif

#include "error.h"
#include "file_io.h"

/*
	WM_Expand_Filename - Get the full root path + filename of a given file
	
	static unsigned char * WM_Expand_Filename (const char *filename)
	
	filename	The name of the file you want the full path/filename of
	
	Returns
		The memory address of the expanded filename
		NULL		Error
		
	NOTE: Calling function will need to free the data once no-longer needed
*/

static inline unsigned char *
WM_Expand_Filename (const char *filename)
{
#ifndef _WIN32
	char *home = NULL;
	struct passwd *pwd_ent;
	char expanded_dir[1024];
#endif

	if (filename == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "NULL Filename", errno);
		return NULL;
	}
	
	/*
		Store the filename locally
	*/
    char *expanded_name = strdup(filename);
    if (expanded_name == NULL) {
		/*
			An error occured when trying to grab some ram
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
        return NULL;
	}
        
    strcpy(expanded_name, filename);

#ifndef _WIN32
	if (strncmp(expanded_name,"~/",2) == 0) {
		/*
			if the filename starts with the home directory shorthand then we want to expand it
		*/
		if ((pwd_ent = getpwuid (getuid ()))) {
			home = pwd_ent->pw_dir;
		} else {
			home = getenv ("HOME");
		}
		if (home) {
			/*
				prefix the home directory to the filename
			*/
			expanded_name = realloc(expanded_name, (strlen(expanded_name) + strlen(home) + 1));
			if (expanded_name == NULL) {
				/*
					An error occured when trying to grab some ram
				*/
				WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
				return NULL;
			}
			memmove((expanded_name + strlen(home)), (expanded_name + 1), (strlen(expanded_name)));
			strncpy (expanded_name, home,strlen(home));
		}
	} else if (expanded_name[0] != '/') {
		/*
			Otherwise if the filename does not start with root path then we want to obtain it
		*/
		getcwd(expanded_dir,1024);
		if (expanded_dir[strlen(expanded_dir)-1] != '/') {
			expanded_dir[strlen(expanded_dir)+1] = '\0';
			expanded_dir[strlen(expanded_dir)] = '/';
		}
	
		/*
			prefix the root directory to the filename
		*/	
		expanded_name = realloc(expanded_name,(strlen(expanded_name) + strlen(expanded_dir) + 1));
		if (expanded_name == NULL) {
			/*
				An error occured when trying to grab some ram
			*/
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
			return NULL;
		}
		memmove((expanded_name + strlen(expanded_dir)), expanded_name, strlen(expanded_name)+1);
		strncpy (expanded_name,expanded_dir,strlen(expanded_dir));
	}
#endif
	/*
		We should now have the full path root now
	*/
	return expanded_name;
}

/*
	WM_Check_File_Exists - Does the file exists where I say it does?
	
	inline int WM_Check_File_Exists (const char *filename)
	
	filename	the name of the file whose existance want to check
	
	Returns
		1	File Exists
		0	File Does Not Exist
		-1	Error
*/

inline int 
WM_Check_File_Exists (const char *filename)
{
	struct stat check_stat;
	/*
		make sure we check with the full filename
	*/
	char *check_file = WM_Expand_Filename(filename);
	if (check_file == NULL) {
		/*
			An error occured when trying to get the full filename
		*/
		return -1;
	}
	
	/*
		Simply obtain file info to see if it exists
	*/
	if (stat(check_file,&check_stat)) {
		/*
			The file does not exist or cannot be accessed
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_STAT, filename, errno);
		free(check_file);
		return 0;
	}
	
	/*
		The file exists
	*/
	free(check_file);
    return 1;	
}

/*
	WM_BufferFile - load a file into memory
	
	inline unsigned char * WM_BufferFile (const char *filename, unsigned long int *size)
	
	filename	name of the file to load
	size		location where to store the size of the file
	
	Returns
		The address where the file is stored and sets size
		NULL		Error
		
	NOTE: Calling function will need to free the data once no-longer needed
*/

inline unsigned char *
WM_BufferFile (const char *filename, unsigned long int *size) {
	int buffer_fd;
	unsigned char *data;
	struct stat buffer_stat;

	/*
		make sure we use the full filename
	*/
	char *buffer_file = WM_Expand_Filename(filename);
	if (buffer_file == NULL) {
		/*
			An error occured when trying to get the full filename
		*/
		return NULL;
	}
	
	if (stat(buffer_file,&buffer_stat)) {
		/*
			The file does not exist or cannot be accessed
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_STAT, filename, errno);
		free(buffer_file);
		return NULL;
	}

	if (buffer_stat.st_size == 0) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, filename, errno);
		free(buffer_file);
		return NULL;
	}
	
	/*
		Store the size of the file
	*/
	*size = buffer_stat.st_size;

	/*
		Grab enough ram to store the file
	*/
	data = (unsigned char *)malloc(*size);
	if (data == NULL) {
		/*
			An error occured when trying to grab some ram
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_STAT, filename, errno);
		free(buffer_file);
		return NULL;
	}

	/*
		Open the file and read it into memory
	*/
#ifdef _WIN32
	if ((buffer_fd = open(buffer_file,(O_RDONLY | O_BINARY))) == -1) {
#else
	if ((buffer_fd = open(buffer_file,O_RDONLY)) == -1) {
#endif
		/*
			An error occured when trying to open the file
		*/
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_OPEN, filename, errno);
		free(buffer_file);
		free(data);
		return NULL;
	}

	if (read(buffer_fd,data,*size) != buffer_stat.st_size) {
		/*
			An error occured when trying to read the file
		*/	
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_READ, filename, errno);
		free(buffer_file);
		free(data);
		close(buffer_fd);
		return NULL;
	}
	
	close(buffer_fd);
	free(buffer_file);
	
	/*
		Return the memory location where the file was stored
	*/
	return data;						
}
