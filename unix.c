#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>

#define NTX_DIR ".ntx"
#define FILE_MAX (FILENAME_MAX+1)

/* Prototypes of utility functions. */
void die(char *fmt, ...);

/* System-dependent functions for POSIX. */
void ntx_editor(char *file)
{
  char *editor = getenv("EDITOR");
  pid_t child;

  if(!editor) die("The environment variable EDITOR is unset.");

  if((child = fork()) == 0) execlp(editor, editor, file, (char*)NULL);
  else waitpid(child, NULL, 0);
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

  if(chdir(ntxroot) == -1) {
    mkdir(ntxroot, S_IRWXU);
    if(chdir(ntxroot) == -1) die("Unable to enter directory %s.\n", ntxroot);
    while((subd = va_arg(args, char *))) {
      if(mkdir(subd, S_IRWXU) == -1)
        die("Cannot make directory %s.\n", subd);
    }
  }
  va_end(args);
}

long int ntx_flen(char *file)
{
  struct stat tmp;
  if(stat(file, &tmp) != 0) return -1;
  return tmp.st_size;
}

DIR *ntx_dopen(char *dir)
{
  return opendir(dir);
}

char *ntx_dread(DIR *dir)
{
  struct dirent *dirp;
  dirp = readdir(dir);
  if(!dirp) return NULL;
  return dirp->d_name;
}

int ntx_dclose(DIR *dir)
{
  return closedir(dir);
}
