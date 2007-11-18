#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <zlib.h>
#include <errno.h>
#include "except.h"

FILE *raw_open(char *file, char *mode)
{
  FILE *f = fopen(file, mode);
  if(!f) throw(E_FACCESS, file);
  resource(f, (resource_handler)fclose);
  return f;
}

char* raw_getl(FILE *f, char *buf, unsigned int max)
{
  char *b = fgets(buf, max, f);
  if(!b && ferror(f) != 0) throw(E_FIOERR, f);
  return b;
}

void *alloc(unsigned int size)
{
  void *buf = malloc(size);
  if(!buf) throw(E_NOMEM, (void*)size);
  resource(buf, free);
  return buf;
}

void *ralloc(void *buf, unsigned int size)
{
  void *tmp = realloc(buf, size);
  if(!tmp) throw(E_NOMEM, (void*)size);
  release_pop(buf, 0);
  resource(tmp, free);
  return tmp;
}

char *strdupe(char *buf)
{
  char *b = strdup(buf);
  if(!b) throw(E_NOMEM, (void*)strlen(buf));
  resource(b, free);
  return b;
}

unsigned int seprintf(char *buf, unsigned int max, char *fmt, ...)
{
  va_list args;
  int len;

  va_start(args, fmt);
  len = vsnprintf(buf, max, fmt, args);
  va_end(args);
  if(len < 0) throw(E_OVRFLO, NULL);
  return (unsigned int)len;
}

gzFile *gzf_open(char *file, char *mode)
{
  gzFile *f = gzopen(file, mode);
  if(!f) {
    if(errno) throw(E_FACCESS, file);
    else throw(E_NOMEM, NULL);
  }
  resource(f, (resource_handler)gzclose);
  return f;
}

unsigned int gzf_write(gzFile *f, void *buf, unsigned int max)
{
  unsigned int len = gzwrite(f, buf, max);
  if(!len && max != len) throw(E_FIOERR, f);
  return len;
}

unsigned int gzf_read(gzFile *f, void *buf, unsigned int max)
{
  int len = gzread(f, buf, max);
  if(len < 0) throw(E_FIOERR, f);
  return (unsigned int)len;
}

unsigned int gzf_putl(gzFile *f, char *buf)
{
  int len = gzputs(f, buf);
  if(len < 0) throw(E_FIOERR, f);
  return (unsigned int)len;
}

char *gzf_getl(gzFile *f, void *buf, unsigned int max)
{
  char *b = gzgets(f, buf, max);
  if(b == Z_NULL && !gzeof(f)) throw(E_FIOERR, f);
  return b;
}
