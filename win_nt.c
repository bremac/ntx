#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>

#define NTX_DIR ".ntx"
#define FILE_MAX (FILENAME_MAX+1)

/* Prototypes of utility functions. */
void die(char *fmt, ...);

/* System-dependent functions for Windows NT and later. */
void ntx_editor(char *file)
{
  char *editor = getenv("EDITOR");

  if(!editor) die("The environment variable EDITOR is unset.");
  _spawnlp(_P_WAIT, editor, editor, file, (char*)NULL);
}

/* The final argument _must_ be NULL. */
void ntx_homedir(char *sub, ...)
{
  va_list args;
  char ntxroot[FILE_MAX];
  char *subd;

  va_start(args, sub);
  if(!snprintf(ntxroot, FILE_MAX, "%s/"NTX_DIR, getenv("HOME")))
    die("Unknown execution failure.\n");

  if(_chdir(ntxroot) == -1) {
    _mkdir(ntxroot, S_IRWXU);
    if(_chdir(ntxroot) == -1) die("Unable to enter directory %s.\n", ntxroot);
    while((subd = va_arg(args, char *))) {
      if(_mkdir(subd, S_IRWXU) == -1)
        die("Cannot make directory %s.\n", subd);
    }
  }
  va_end(args);
}

long int ntx_flen(char *file)
{
  int fd = _open(file, _O_RDONLY);
  long int len;

  if(!fd) return -1;
  len = _filelength(fd);
  _close(fd);
  return len;
}


/*
 *  Based off of the dirent for win32 code; Copyright notice follows.
 *
 *  The original source can be found at the following web address:
 *  http://www.two-sdg.demon.co.uk/curbralan/code/dirent/dirent.html
 */

/*
    Copyright Kevlin Henney, 1997, 2003. All rights reserved.

    Permission to use, copy, modify, and distribute this software and its
    documentation for any purpose is hereby granted without fee, provided
    that this copyright and permissions notice appear in all copies and
    derivatives.
    
    This software is supplied "as is" without express or implied warranty.

    But that said, if there are any problems please get in touch.
*/

typedef struct
{
  long                handle;  /* -1 for failed rewind */
  struct _finddata_t  info;
  char                *result; /* null iff first time */
  char                *name;   /* null-terminated char string */
} nDIR;

nDIR *ntx_dopen(char *name)
{
  nDIR *dir = NULL;

  if(name && name[0]) {
    size_t base_length = strlen(name);
    /* search pattern must end with suitable wildcard */
    const char *all = strchr("/\\", name[base_length - 1]) ? "*" : "/*";

    if((dir = (nDIR *) malloc(sizeof *dir)) != 0 &&
       (dir->name = (char *) malloc(base_length + strlen(all) + 1)) != 0) {
      strcat(strcpy(dir->name, name), all);

      if((dir->handle = (long) _findfirst(dir->name, &dir->info)) != -1)
        dir->result = NULL;
      else {
        free(dir->name);
        free(dir);
        dir = NULL;
      }
    } else {
      free(dir);
      dir = NULL;
      die("Out of memory.\n");
    }
  } else die("Invalid directory %s\n", name);

  return dir;
}

char *ntx_dread(nDIR *dir)
{
  struct dirent *result = 0;

  if(dir && dir->handle != -1) {
    if(!dir->result || _findnext(dir->handle, &dir->info) != -1)
      return dir->info.name;
  } else die("Unknown execution error.\n");

  return NULL; /* Not reached. */
}

int ntx_dclose(nDIR *dir)
{
  if(dir) {
    if(dir->handle != -1) result = _findclose(dir->handle);

    free(dir->name);
    free(dir);
  } else return -1;

  return 0;
}
