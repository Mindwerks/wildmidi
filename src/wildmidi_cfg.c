/*
    wildmidi_cfg.c - Template for creating all files in CVS
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

    $Id: wildmidi_cfg.c,v 1.3 2008/11/03 00:43:28 wildcode Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_io.h"
#include "wildmidi_cfg.h"

/*
	tokenize_line - break the line up by words
	
	unsigned char **tokenize_line (char *line)
	
	line		The line of text to break up
	
	Returns
		pointer to an array containing pointers to each word in the line
		NULL		Nothing to break up
*/
static inline unsigned char **
tokenize_line (char *line) {
	unsigned char **tokens = NULL;
	unsigned long int token_count =0;
	unsigned long int line_ofs = 0;
	unsigned long int store_token = 0;
	unsigned char line_chr; = 0;
	
	if (strlen(line) == 0) {
		/*
			Nothing in the line to tokenize
		*/
		return NULL;
	}
	
	line_chr = line[line_ofs];
	do {
		if ((line_chr == '\0') || (line_chr == '#')) {
			/*
				start of comment or delimiter
			*/
			break;
		}
		
		if (line_chr == ' ') {
			if (store_token) {
				/* 
					we are at the end of a token so change space to a delimiter 
				*/
				line[line_ofs] = '\0';
				/*
					and record that we are not in a token
				*/
				store_token = 0;
			}
		} else {
			if (!store_token) {
				/*
					we found a new token
				*/
				tokens = realloc(tokens, ((token_count + 1) * sizeof(char *)));
				tokens[token_count] = &line[line_ofs];
				token_count++;
				store_token = 1;
			}
		}
		line_ofs++;
		line_chr = line[line_ofs];
	} while (line_chr != '\0');
	
	/*
		add a marker to the end of the tokens
	*/
    tokens = realloc(tokens, ((token_count + 1) * sizeof(char *)));
    tokens[token_count] = malloc(1);
    tokens[token_count][0] = '\0';

	return tokens;
}

/*
	WM_FreePatchcfg - free memory used to store  patch configuration data
	
	WM_FreePatchcfg (struct _patchcfg * patchcfgs) 
	
	patchcfgs		Address of the first config in the chain
	
	Returns nothing
*/
inline void
WM_FreePatchcfg (struct _patchcfg * patchcfgs) {
	struct _patchcfg * tmp_patchcfgs = NULL;
	struct _patchcfg * next_patchcfgs = NULL;
	
	if (patchcfgs == NULL) {
		return;
	}
	
	tmp_patchcfgs = patchcfgs;
	do {
		next_patchcfgs = tmp_patchcfgs->next;
		free(tmp_patchcfgs->filename);
		free(tmp_patchcfgs);
		tmp_patchcfgs = next_patchcfgs;
	} while (tmp_patchcfgs != NULL);
}

static inline void
parse_extra_config (struct _patchcfg *config, char **tokens) {
	int tmp_count = 0;
		
	config->amp = 0;
	config->note = 0;
	config->pan = 0;
	config->keep = 0;
	
	if (tokens[2][0] = '\0') {
		tmp_count = 2;
		do {
			if (strncmp(tokens[tmp_count], "amp=", 4) == 0) {
			} else if (strncmp(tokens[tmp_count], "note=", 5) == 0) {
			} else if (strncmp(tokens[tmp_count], "pan=", 4) == 0) {
				if (strncmp(tokens[tmp_count], "pan=center", 10) {
				} else if (strncmp(tokens[tmp_count], "pan=left", 8) {
				} else if (strncmp(tokens[tmp_count], "pan=right", 9) {
				} else {
					//TODO: Pan Setting
				}
			} else if (strncmp(tokens[tmp_count], "keep=env", 8) == 0) {
				config->keep |= KEEP_ENV;
			} else if (strncmp(tokens[tmp_count], "keep=loop", 9) == 0) {
				config->keep |= KEEP_LOOP;
			} else if (strncmp(tokens[tmp_count], "remove=sustain", 14) == 0) {
				config->keep |= REMOVE_SUSTAIN;
			} else if (strncmp(tokens[tmp_count], "env_time=", 9) == 0) {
			//TODO
			} else if (strncmp(tokens[tmp_count], "env_level=", 10) == 0) {
			//TODO
			} else if (strncmp(tokens[tmp_count], "strip=env", 9) == 0) {
			} else if (strncmp(tokens[tmp_count], "strip=loop", 10) == 0) {
			} else if (strncmp(tokens[tmp_count], "strip=tail", 10) == 0) {
			}
			tmp_count++;
		} while (tokens[tmp_count][0] != '\0');
	}
	
	return;
}

/*
	WM_LoadConfig - load timidity.cfg file
	
	struct _patchcfg *WM_LoadConfig (const char *config_file)
	
	config_file		filemane of the config file to load
	
	Returns
		The address to the patch configurations
		NULL		Error

	NOTE	: Calling function will need to free the data once no-longer needed
			: This function is recursive
		
	==============================================
	
	dir					directory	The path where the pat files are located
	
	source				another config file to parse
	
	bank [number]			Bank number of the following patches
	
	drumset [number]			Drumset number of the following patches
	
	[number] [file] [options]		Patch number, guspat file and sample options
	
		amp=[percent]		amplification of the guspat sample
		note=[note]			note to play the sample at
		pan=[pan]			default pan
		keep=[loop|env]		
		strip=[loop|env|tail]
	==============================================
*/
inline struct _patchcfg *
WM_LoadConfig (const char *config_file) {
	struct _patchcfg *ret_patchcfgs = NULL;
	struct _patchcfg *ptr_patchcfgs = NULL;
	struct _patchcfg *tmp_a_patchcfgs = NULL;
	struct _patchcfg *tmp_b_patchcfgs = NULL;
	struct _patchcfg *prev_patchcfgs = NULL;
	struct _patchcfg *new_patchcfg = NULL;
	
	unsigned char *config_buffer = NULL;
	unsigned long int config_ofs = 0;
	unsigned char *config_ptr = 0;
	unsigned long int config_size = 0;
	unsigned char config_chr = 0;
	unsigned char **tokens = NULL;
	unsigned long int token_count =0;
	unsigned char *new_config = NULL;
	unsigned char *dir = NULL;
	unsigned short patchid = 0;
	char *tmp_filename = NULL;
	
	/*
		Load the config file
	*/
	if ((config_buffer = WM_BufferFile(config_file, &config_size)) == NULL) {
		return NULL;
	}
	
	if (strrchr(config_file,'/')) {
		int tmp_len = strrchr(config_file,'/') - config_file;
		dir = malloc(tmp_len + 1);
		memcpy(dir,config_file,tmp_len);
		dir[tmp_len] = '\0';
	} else if (strrchr(config_file,'\\')) {
		int tmp_len = strrchr(config_file,'\\') - config_file;
		dir = malloc(tmp_len + 1);
		memcpy(dir,config_file,tmp_len);
		dir[tmp_len] = '\0';
	}
	
	config_buffer = realloc(config_buffer, (config_size + 1));
	config_buffer[config_size] = '\0';
	
	/*
		Remove any tabs and line returns
	*/
	config_ofs = 0;
    config_chr = config_buffer[config_ofs];
	do {
		if ((config_chr == '\t') || (config_chr == '\r')) {
			config_buffer[config_ofs] = ' ';
		} else if (config_chr == '\n') {
			config_buffer[config_ofs] = '\0';
		}
		config_ofs++;
		config_chr = config_buffer[config_ofs];
	} while (config_chr != '\0');
	
	config_ofs = 0;
	do {
		config_ptr = strchr(&config_buffer[config_ofs], '\0');
		/*
			tokenize the line for easier parsing
		*/
		if ((tokens = tokenize_line(&config_buffer[config_ofs]))) {
			/*
				Grab Token Count
			*/
			token_count = 0;
			while (tokens[token_count][0] != '\0') {
				token_count++;
			}
			
			/*
				Parse Tokens
			*/
			if (strcmp(tokens[0],"dir") == 0) {
				/*
					token is a directory entry. Set directory to this
				*/
				if (dir != NULL) {
					free (dir);
				}
				dir = strdup(tokens[1]);
			} else if (strcmp(tokens[0],"source") == 0) {
				/*
					token is a config file entry, create temp filename
				*/
				if (dir != NULL) {
					/*
						Add directory to config filename
					*/
					if ((dir[(strlen(dir) - 1)] == '/') || (dir[(strlen(dir) - 1)] == '\\')) {
						/*
							directory ends in / or \
						*/
						new_config = malloc(strlen(dir)+strlen(tokens[1])+1);
						memcpy(new_config, dir, strlen(dir));
						memcpy(&new_config[strlen(dir)], tokens[1], strlen(tokens[1]));
						new_config[(strlen(dir)+strlen(tokens[1]))] = '\0';
					} else {
						/*
							directory does not end in / or \ so we add it.
						*/
						new_config = malloc(strlen(dir)+1+strlen(tokens[1])+1);
						memcpy(new_config, dir, strlen(dir));
						memcpy((&new_config[strlen(dir)] + 1), tokens[1], strlen(tokens[1]));
						new_config[strlen(dir)] = '/';
						new_config[(strlen(dir)+1+strlen(tokens[1]))] = '\0';
					}
				} else {
					/*
						No directory entry so config filename remains unchanged
					*/
					new_config = strdup(tokens[1]);
				}
				
				/*
					Parse config file
				*/
				if ((ptr_patchcfgs = WM_LoadConfig(new_config)) != NULL) {
					/*
						config file contained patch entries
					*/
					if (ret_patchcfgs == NULL) {
						/*
							There are currently no patch entries so returned patch data doesn't need parsing.
						*/
						ret_patchcfgs = ptr_patchcfgs;
					} else {
						tmp_a_patchcfgs = ptr_patchcfgs;
						do {
							tmp_b_patchcfgs = ret_patchcfgs;
							prev_patchcfgs = NULL;
							new_patchcfg = NULL;
							do {
								if (tmp_a_patchcfgs->patchid == tmp_b_patchcfgs->patchid) {
									/*
										patch entry exists, modify patch list entry
									*/
									tmp_b_patchcfgs->filename = realloc (tmp_b_patchcfgs->filename, (strlen(tmp_a_patchcfgs->filename) + 1));
									memcpy(tmp_b_patchcfgs->filename, tmp_a_patchcfgs->filename, (strlen(tmp_a_patchcfgs->filename) + 1));
									new_patchcfg = tmp_b_patchcfgs;
									break;
								} else if (tmp_a_patchcfgs->patchid < tmp_b_patchcfgs->patchid) {
									/*
										patch entry needs to be inserted
									*/
									if (prev_patchcfgs == NULL) {
										prev_patchcfgs = malloc(sizeof(struct _patchcfg));
										ret_patchcfgs = prev_patchcfgs;
									} else {
										prev_patchcfgs->next = malloc(sizeof(struct _patchcfg));
										prev_patchcfgs = prev_patchcfgs->next;
									}
									prev_patchcfgs->next = tmp_b_patchcfgs;
									prev_patchcfgs->patchid = tmp_a_patchcfgs->patchid;
									prev_patchcfgs->filename = malloc(strlen(tmp_a_patchcfgs->filename) + 1);
									memcpy(prev_patchcfgs->filename, tmp_a_patchcfgs->filename, (strlen(tmp_a_patchcfgs->filename) + 1));
									
									break;
								}
								prev_patchcfgs = tmp_b_patchcfgs;
								tmp_b_patchcfgs = tmp_b_patchcfgs->next;
								if (tmp_b_patchcfgs == NULL) {
									/*
										no suitable patch entry exists, append to patch list
									*/
									prev_patchcfgs->next = malloc(sizeof(struct _patchcfg));
									prev_patchcfgs = prev_patchcfgs->next;
									prev_patchcfgs->next = NULL;
									prev_patchcfgs->patchid = tmp_a_patchcfgs->patchid;
									prev_patchcfgs->filename = malloc(strlen(tmp_a_patchcfgs->filename) + 1);
									memcpy(prev_patchcfgs->filename, tmp_a_patchcfgs->filename, (strlen(tmp_a_patchcfgs->filename) + 1));
									
									break;
								}
							} while (tmp_b_patchcfgs != NULL);
							tmp_a_patchcfgs = tmp_a_patchcfgs->next;
						} while (tmp_a_patchcfgs != NULL);
						WM_FreePatchcfg (ptr_patchcfgs);
					}
				}
				/*
					remove temp filename
				*/
				free (new_config);
			} else if (strcmp(tokens[0],"bank") == 0) {
				if (!isdigit(tokens[1][0])) {
					/*ERROR*/
				} else {
					patchid = ((atoi(tokens[1]) & 0x7f) << 8);
				}
			} else if (strcmp(tokens[0],"drumset") == 0) {
				if (!isdigit(tokens[1][0])) {
					/*ERROR*/
				} else {
					patchid = ((atoi(tokens[1]) & 0x7f) << 8) | 0x80;
				}
			} else if (isdigit(tokens[0][0])) {
				if (token_count < 2) {
					/*ERROR*/
				} else {
					patchid = (patchid & 0xFF80) | (atoi(tokens[0]) & 0x7f);
					
					if (dir != NULL) {
						if ((dir[(strlen(dir) - 1)] == '/') || (dir[(strlen(dir) - 1)] == '\\')) {
						/*
							directory ends in / or \
						*/
						tmp_filename = malloc(strlen(dir)+strlen(tokens[1])+1);
						memcpy(tmp_filename, dir, strlen(dir));
						memcpy(&tmp_filename[strlen(dir)], tokens[1], strlen(tokens[1]));
						tmp_filename[(strlen(dir)+strlen(tokens[1]))] = '\0';
						} else {
						/*
							directory does not end in / or \ so we add it.
						*/
						tmp_filename = malloc(strlen(dir)+1+strlen(tokens[1])+1);
						memcpy(tmp_filename, dir, strlen(dir));
						memcpy((&tmp_filename[strlen(dir)] + 1), tokens[1], strlen(tokens[1]));
						tmp_filename[strlen(dir)] = '/';
						tmp_filename[(strlen(dir)+1+strlen(tokens[1]))] = '\0';
						}
					} else {
						tmp_filename = malloc(strlen(tokens[1]) + 1);
						memcpy(tmp_filename,tokens[1],strlen(tokens[1]));
						tmp_filename[strlen(tokens[1])] = '\0';
					}
					if (strncasecmp(&tmp_filename[strlen(tmp_filename) - 4], ".pat", 4) != 0)
                    {
                        tmp_filename = realloc(tmp_filename, strlen(tmp_filename) + 5);
					    strcat(tmp_filename, ".pat");                                       
                    }
					
					if (WM_Check_File_Exists(tmp_filename)) {
						/*
							File exists so add it to the patch list
						*/
						if (ret_patchcfgs == NULL) {
							/*
								There are currently no patch entries so returned patch data doesn't need parsing.
							*/
							ret_patchcfgs = malloc(sizeof(struct _patchcfg));
							ret_patchcfgs->patchid = patchid;
							ret_patchcfgs->filename = tmp_filename;
							ret_patchcfgs->next = NULL;
							
						} else {
							tmp_a_patchcfgs = ret_patchcfgs;
							prev_patchcfgs = NULL;
							do {
								if (tmp_a_patchcfgs->patchid == patchid) {
									tmp_a_patchcfgs->filename = realloc(tmp_a_patchcfgs->filename,(strlen(tmp_filename) + 1));
									memcpy(tmp_a_patchcfgs->filename, tmp_filename, (strlen(tmp_filename) + 1));
									
									break;
								} else if (tmp_a_patchcfgs->patchid > patchid)  {
									/*
										patch entry needs to be inserted
									*/
									if (prev_patchcfgs == NULL) {
										prev_patchcfgs = malloc(sizeof(struct _patchcfg));
										ret_patchcfgs = prev_patchcfgs;
									} else {
										prev_patchcfgs->next = malloc(sizeof(struct _patchcfg));
										prev_patchcfgs = prev_patchcfgs->next;
									}
									prev_patchcfgs->next = tmp_a_patchcfgs;
									prev_patchcfgs->patchid = patchid;
									prev_patchcfgs->filename = malloc(strlen(tmp_filename) + 1);
									memcpy(prev_patchcfgs->filename, tmp_filename, (strlen(tmp_filename) + 1));
									
									break;
								}
								prev_patchcfgs = tmp_a_patchcfgs;
								tmp_a_patchcfgs = tmp_a_patchcfgs->next;
								if (tmp_a_patchcfgs == NULL) {
									prev_patchcfgs->next = malloc(sizeof(struct _patchcfg));
									prev_patchcfgs = prev_patchcfgs->next;
									prev_patchcfgs->next = NULL;
									prev_patchcfgs->patchid = patchid;
									prev_patchcfgs->filename = malloc(strlen(tmp_filename) + 1);
									memcpy(prev_patchcfgs->filename, tmp_filename, (strlen(tmp_filename) + 1));
									break;
								}
							} while (tmp_a_patchcfgs != NULL);
						} 
					}
					
				}
			}
		}
		if (config_ptr == NULL) {
			break;
		};
		config_ofs = config_ptr - config_buffer + 1;
		free (tokens);
	} while (config_ofs < config_size);
	
	free (config_buffer);
	return ret_patchcfgs;
}

#if 0

char **
WM_LC_Tokenize_Line (char * line_data)
{
    int line_length = strlen(line_data);
    int line_ofs = 0;
    int token_start = -1;
    char **token_data = NULL;
    int token_count = 0;
    if (line_length != 0)
    {
        while (line_ofs != line_length)
        {
            /*
                we ignore everything after # in a line
            */
            if (line_data[line_ofs] == '#')
                break;
                  
            /*
                spaces or tabs are the token seperators
            */    
            if ((line_data[line_ofs] == ' ') || (line_data[line_ofs] == '\t'))
            {
                /*
                    if a token was found, record it when we hit a space or a tab
                */
                if (token_start != -1)
                {
                                
                    token_data = realloc(token_data, ((token_count + 1) * sizeof(char *)));
                    token_data[token_count] = malloc(line_ofs - token_start + 1);
                    strncpy(token_data[token_count], &line_data[token_start], (line_ofs - token_start));
                    token_data[token_count][(line_ofs - token_start)] = '\0';
                    token_count++;
                    token_start = -1;
                }
            } else {
                /*
                   if we haven't marked the start of a token, then mark it
                */   
                if (token_start == -1)
                    token_start = line_ofs;
            }
            line_ofs++;
        }
    }
    
    /*
       no more data to scan through so if we had marked the start of a token, record it
    */
    if (token_start)
    {
        token_data = realloc(token_data, ((token_count + 1) * sizeof(char *)));
        token_data[token_count] = malloc(line_ofs - token_start + 1);
        strncpy(token_data[token_count], &line_data[token_start], (line_ofs - token_start));
        token_data[token_count][(line_ofs - token_start)] = '\0';
        token_count++;
        token_start = 0;
    }
    
    /*
       add a \0 token to mark the end of the token list
    */
    token_data = realloc(token_data, ((token_count + 1) * sizeof(char *)));
    token_data[token_count] = malloc(1);
    token_data[token_count][0] = '\0';
    return token_data;
}

void
free_tokens (char **line_tokens)
{
    int token_count = 0;
    while (line_tokens[token_count][0] != '\0')
    {
          free(line_tokens[token_count]);
          token_count++;
    }
    free(line_tokens[token_count]);
    free(line_tokens);
}

struct _direntry {
    char *dir;
    int cfg_id;
    struct _direntry *prev; 
    struct _direntry *next;
     
} *direntry;

int dir_count = 0;

void
free_direntry (void)
{
    struct _direntry *tmp_direntry;
    if (dir_count)
    {
        do
        {
            tmp_direntry = direntry->next;
            free(direntry);
            direntry = tmp_direntry;      
            dir_count--;        
        } while (dir_count);
    }
}

inline int
WM_LoadConfig (const char *config_file)
{
	unsigned long int config_size = 0;
	unsigned char *config_buffer =  NULL;
	char * dir_end =  NULL;
	char * config_dir =  NULL;
	unsigned long int config_ptr = 0;
	unsigned long int line_start_ptr = 0;
	unsigned short int patchid = 0;
	char * new_config = NULL;
	struct _patch * tmp_patch;
	char **line_tokens = NULL;
	int token_count = 0;
	

	if ((config_buffer = WM_BufferFile(config_file, &config_size)) == NULL)
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
                line_tokens = WM_LC_Tokenize_Line(&config_buffer[line_start_ptr]);
                if (strcasecmp(line_tokens[0],"dir") == 0)
                {
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
					    free_tokens (line_tokens);
					    free (config_buffer);
					    return -1;
				    }
                    if (config_dir[strlen(config_dir) - 1] != '/')
                    {
                        config_dir = realloc(config_dir,(strlen(config_dir) + 2));
				        config_dir[strlen(config_dir) + 1] = '\0';
				        config_dir[strlen(config_dir)] = '/';
                    }
                } else if (strcasecmp(line_tokens[0],"source") == 0)
                {
                    if ((line_tokens[1][0] == '/') || (line_tokens[1][0] == '~'))
                    {
                        new_config = malloc(strlen(line_tokens[1]) + 1);
				        if (new_config == NULL)
                        {
                            WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse config", errno);
					        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
					        WM_FreePatches();
					        free_tokens (line_tokens);
					        free (config_buffer);
					        return -1;
				        }
				        strcpy(new_config, line_tokens[1]);                   
                    } else if (config_dir != NULL)
                    {
				        new_config = malloc(strlen(config_dir) + strlen(line_tokens[1]) + 1);
				        if (new_config == NULL)
                        {
					        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse config", errno);
					        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
					        WM_FreePatches();
					        free (config_dir);	
					        free_tokens (line_tokens);
					        free (config_buffer);
					        return -1;
				        }
				        strcpy(new_config,config_dir);
				        strcpy(&new_config[strlen(config_dir)], line_tokens[1]);
			        } else
                    {
				        new_config = malloc(strlen(line_tokens[1]) + 1);
				        if (new_config == NULL)
                        {
                            WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse config", errno);
					        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
					        WM_FreePatches();
					        free_tokens (line_tokens);
					        free (config_buffer);
					        return -1;
				        }
				        strcpy(new_config, line_tokens[1]);
		            }
			        if (WM_LoadConfig(new_config) == -1)
                    {
				        free (new_config);
				        free_tokens (line_tokens);
				        free (config_buffer);
				        if (config_dir != NULL)
					        free (config_dir);
				        return -1;
			        }
			        free (new_config);
                } else if (strcasecmp(line_tokens[0],"bank") == 0)
                {
			        if (!isdigit(line_tokens[1][0]))
                    {
                        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in bank line)", 0);
				        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
				        WM_FreePatches();
				        if (config_dir != NULL)
					        free (config_dir);
				        free_tokens (line_tokens);
				        free (config_buffer);
				        return -1;
			        }
                    patchid = (atoi(line_tokens[1]) & 0xFF ) << 8;
                } else if (strcasecmp(line_tokens[0],"drumset") == 0)
                {
			        if (!isdigit(line_tokens[1][0]))
                    {
                        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in drumset line)", 0);
				        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
				        WM_FreePatches();
				        if (config_dir != NULL)
					        free (config_dir);
				        free_tokens (line_tokens);
				        free (config_buffer);
				        return -1;
			        }
                    patchid = ((atoi(line_tokens[1]) & 0xFF ) << 8) | 0x80;
                } else if (isdigit(line_tokens[0][0]))
                {
                    patchid = (patchid & 0xFF80) | (atoi(line_tokens[0]) & 0x7F);
			        if (patch[(patchid & 0x7F)] == NULL)
                    {
				        patch[(patchid & 0x7F)] = malloc (sizeof(struct _patch));
				        if (patch[(patchid & 0x7F)] == NULL) 
                        {
					        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
					        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
					        WM_FreePatches();
					        if (config_dir != NULL)
						        free (config_dir);	
			                free_tokens (line_tokens);
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
			        } else 
                    {
				        tmp_patch = patch[(patchid & 0x7F)];
				        if (tmp_patch->patchid == patchid)
                        {
					        free (tmp_patch->filename);
					        tmp_patch->filename = NULL;
					        tmp_patch->amp = 1024;
					        tmp_patch->note = 0; 
				        } else
                        {
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
							        tmp_patch->next = malloc (sizeof(struct _patch));
							        if (tmp_patch->next == NULL)
                                    {
								        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, 0);
								        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
								        WM_FreePatches();
								        if (config_dir != NULL)
									        free (config_dir);	
								        free_tokens (line_tokens);
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
						        } else
                                {
							        tmp_patch = tmp_patch->next;
							        free (tmp_patch->filename);
							        tmp_patch->filename = NULL;
							        tmp_patch->amp = 1024;
							        tmp_patch->note = 0; 
						        }
					        } else
                            {
						        tmp_patch->next = malloc (sizeof(struct _patch));
						        if (tmp_patch->next == NULL)
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
							        WM_FreePatches();
							        if (config_dir != NULL)
								        free (config_dir);	
							        free_tokens (line_tokens);
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
						    free_tokens (line_tokens);
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
						    free_tokens (line_tokens);
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
						    free_tokens (line_tokens);
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
                    while (line_tokens[token_count][0] != '\0')
                    {
                        if (strncasecmp(line_tokens[token_count], "amp=", 4) == 0)
                        {
                            if (!isdigit(line_tokens[token_count][4]))
                            {
                                WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
                            } else
                            {
                                tmp_patch->amp = (atoi(&line_tokens[token_count][4]) << 10) / 100;   
                            }       
                        } else if (strncasecmp(line_tokens[token_count], "note=", 5) == 0)
                        {
                            if (!isdigit(line_tokens[token_count][5]))
                            {
                                WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
                            } else
                            {
                                tmp_patch->note = (atoi(&line_tokens[token_count][5]) << 10) / 100;   
                            }       
                        } else if (strncasecmp(line_tokens[token_count], "env_time0=", 10) == 0)
                        {
					        if (!isdigit(line_tokens[token_count][10]))
                            {
					            WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
					        } else
                            {
				                tmp_patch->env[0].time = atof(&line_tokens[token_count][10]);
						        if ((tmp_patch->env[0].time > 45000.0) || (tmp_patch->env[0].time < 1.47))
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
							        tmp_patch->env[0].set &= 0xFE;
						        } else
                                {
							        tmp_patch->env[0].set |= 0x01;
						        }
					        }
                        } else if (strncasecmp(line_tokens[token_count], "env_level0=", 11) == 0)
                        {
					        if (!isdigit(line_tokens[token_count][11])) {
						        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
					        } else
                            {
				                tmp_patch->env[0].level = atof(&line_tokens[token_count][11]);
						        if ((tmp_patch->env[0].level > 1.0) || (tmp_patch->env[0].level < 0.0))
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
							        tmp_patch->env[0].set &= 0xFD;
						        } else
                                {
							        tmp_patch->env[0].set |= 0x02;
						        }
					        }
                        } else if (strncasecmp(line_tokens[token_count], "env_time1=", 10) == 0)
                        {
					        if (!isdigit(line_tokens[token_count][10]))
                            {
					            WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
					        } else
                            {
				                tmp_patch->env[1].time = atof(&line_tokens[token_count][10]);
						        if ((tmp_patch->env[1].time > 45000.0) || (tmp_patch->env[1].time < 1.47))
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
							        tmp_patch->env[1].set &= 0xFE;
					            } else
                                {
							        tmp_patch->env[1].set |= 0x01;
						        }
					        }
                        } else if (strncasecmp(line_tokens[token_count], "env_level1=", 11) == 0)
                        {
					        if (!isdigit(line_tokens[token_count][11]))
                            {
						        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
					        } else
                            {
				                tmp_patch->env[1].level = atof(&line_tokens[token_count][11]);
						        if ((tmp_patch->env[1].level > 1.0) || (tmp_patch->env[1].level < 0.0))
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
							        tmp_patch->env[1].set &= 0xFD;
						        } else
                                {
							        tmp_patch->env[1].set |= 0x02;
						        }
					        }
                        } else if (strncasecmp(line_tokens[token_count], "env_time2=", 10) == 0)
                        {
					        if (!isdigit(line_tokens[token_count][10]))
                            {
						        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
					        } else
                            {
				                tmp_patch->env[2].time = atof(&line_tokens[token_count][10]);
						        if ((tmp_patch->env[2].time > 45000.0) || (tmp_patch->env[2].time < 1.47))
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
							        tmp_patch->env[2].set &= 0xFE;
						        } else
                                {
							        tmp_patch->env[2].set |= 0x01;
						        }
					        }
                        } else if (strncasecmp(line_tokens[token_count], "env_level2=", 11) == 0)
                        {
					        if (!isdigit(line_tokens[token_count][11]))
                            {
						        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
					        } else
                            {
				                tmp_patch->env[2].level = atof(&line_tokens[token_count][11]);
						        if ((tmp_patch->env[2].level > 1.0) || (tmp_patch->env[2].level < 0.0))
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
							        tmp_patch->env[2].set &= 0xFD;
						        } else
                                {
							        tmp_patch->env[2].set |= 0x02;
						        }
					        }
                        } else if (strncasecmp(line_tokens[token_count], "env_time3=", 10) == 0)
                        {
					        if (!isdigit(line_tokens[token_count][10]))
                            {
						        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
					        } else
                            {
				                tmp_patch->env[3].time = atof(&line_tokens[token_count][10]);
						        if ((tmp_patch->env[3].time > 45000.0) || (tmp_patch->env[3].time < 1.47))
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
							        tmp_patch->env[3].set &= 0xFE;
						        } else
                                {
							        tmp_patch->env[3].set |= 0x01;
						        }
					        }
                        } else if (strncasecmp(line_tokens[token_count], "env_level3=", 11) == 0)
                        {
					        if (!isdigit(line_tokens[token_count][11]))
                            {
						        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
					        } else
                            {
				                tmp_patch->env[3].level = atof(&line_tokens[token_count][11]);
						        if ((tmp_patch->env[3].level > 1.0) || (tmp_patch->env[3].level < 0.0))
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
							        tmp_patch->env[3].set &= 0xFD;
						        } else
                                {
							        tmp_patch->env[3].set |= 0x02;
						        }
					        }
                        } else if (strncasecmp(line_tokens[token_count], "env_time4=", 10) == 0)
                        {
					        if (!isdigit(line_tokens[token_count][10]))
                            {
						        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
					        } else {
				                tmp_patch->env[4].time = atof(&line_tokens[token_count][10]);
						        if ((tmp_patch->env[4].time > 45000.0) || (tmp_patch->env[4].time < 1.47))
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
							        tmp_patch->env[4].set &= 0xFE;
						        } else
                                {
							        tmp_patch->env[4].set |= 0x01;
						        }
					        }
                        } else if (strncasecmp(line_tokens[token_count], "env_level4=", 11) == 0)
                        {
					        if (!isdigit(line_tokens[token_count][11]))
                            {
						        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
					        } else
                            {
				                tmp_patch->env[4].level = atof(&line_tokens[token_count][11]);
						        if ((tmp_patch->env[4].level > 1.0) || (tmp_patch->env[4].level < 0.0))
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
							        tmp_patch->env[4].set &= 0xFD;
						        } else
                                {
							        tmp_patch->env[4].set |= 0x02;
						        }
					        }
                        } else if (strncasecmp(line_tokens[token_count], "env_time5=", 10) == 0)
                        {
					        if (!isdigit(line_tokens[token_count][10]))
                            {
						        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
					        } else
                            {
				                tmp_patch->env[5].time = atof(&line_tokens[token_count][10]);
						        if ((tmp_patch->env[5].time > 45000.0) || (tmp_patch->env[5].time < 1.47))
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
							        tmp_patch->env[5].set &= 0xFE;
						        } else
                                {
							        tmp_patch->env[5].set |= 0x01;
						        }
					        }
                        } else if (strncasecmp(line_tokens[token_count], "env_level5=", 11) == 0)
                        {
					        if (!isdigit(line_tokens[token_count][11]))
                            {
						        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(syntax error in patch line)", 0);
					        } else
                            {
				                tmp_patch->env[5].level = atof(&line_tokens[token_count][11]);
						        if ((tmp_patch->env[5].level > 1.0) || (tmp_patch->env[5].level < 0.0))
                                {
							        WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(range error in patch line)", 0);
							        tmp_patch->env[5].set &= 0xFD;
						        } else 
                                {
							        tmp_patch->env[5].set |= 0x02;
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
                /*
                   free up tokens
                */
                free_tokens(line_tokens);
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

#endif
