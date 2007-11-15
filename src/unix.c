#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include "except.h"
#include "exc_io.h"

#define NTX_DIR     ".ntx"
#define BUFFER_LEN  8096
#define FILE_MAX    (FILENAME_MAX+1)

/* Prototypes of utility functions. */
void die(char *fmt, ...);

/* System-dependent functions for POSIX. */
void ntx_editor(char *file)
{
  if(isatty(STDIN_FILENO)) {
    /* Open an editor if we're in interactive mode. */
    char *editor = getenv("EDITOR");
    pid_t child;

    if(editor) {
      if((child = fork()) == 0) execlp(editor, editor, file, (char*)NULL);
      else waitpid(child, NULL, 0);
    } else die("The environment variable EDITOR is unset.\n");
  } else {
    FILE *f = raw_open(file, "w");
    char buffer[BUFFER_LEN];

    /* Copy data from stdin to the target file. */
    while(fgets(buffer, BUFFER_LEN, stdin)) fputs(buffer, f);
    release(f);
  }
}

/* The final argument _must_ be NULL. */
void ntx_homedir(char *sub, ...)
{
  va_list args;
  char ntxroot[FILE_MAX], *path;
  char *subd;

  va_start(args, sub);
  if(!(path = getenv("NTXROOT"))) {
    seprintf(ntxroot, FILE_MAX, "%s/"NTX_DIR, getenv("HOME"));
    path = ntxroot;
  }

  if(chdir(path) == -1) {
    mkdir(path, S_IRWXU);
    if(chdir(path) == -1) die("Unable to enter directory %s.\n", path);
    for(subd = sub; subd; subd = va_arg(args, char *))
      if(mkdir(subd, S_IRWXU) == -1) die("Cannot make directory %s.\n", subd);
  }
  va_end(args);
}

long int ntx_flen(char *file)
{
  struct stat tmp;
  if(stat(file, &tmp) != 0) throw(E_FACCESS, file);
  return tmp.st_size;
}

DIR *ntx_dopen(char *dir)
{
  DIR *d = opendir(dir);
  if(!d) throw(E_FACCESS, dir);
  return d;
}

char *ntx_dread(DIR *dir)
{
  struct dirent *dirp;
  dirp = readdir(dir);
  if(!dirp) return NULL;
  return dirp->d_name;
}

void ntx_dclose(DIR *dir)
{
  closedir(dir);
}
