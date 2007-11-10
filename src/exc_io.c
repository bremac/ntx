#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <zlib.h>
#include "except.h"

/* XXX: Interpret errno, etc. */

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
  if(!b && ferror(f) != 0) throw(E_FINVAL, f);
  return b;
}

void *alloc(unsigned int size)
{
  void *buf = malloc(size);
  if(!buf) throw(E_NOMEM, (void*)size);
  return buf;
}

void *ralloc(void *buf, unsigned int size)
{
  void *tmp = realloc(buf, size);
  if(!tmp) throw(E_NOMEM, (void*)size);
  return tmp;
}

char *strdupe(char *buf)
{
  char *b = strdup(buf);
  if(!b) throw(E_NOMEM, (void*)strlen(buf));
  return b;
}

unsigned int seprintf(char *buf, unsigned int max, char *fmt, ...)
{
  va_list args;
  int len;

  va_start(args, fmt);
  len = vsnprintf(buf, max, fmt, args);
  va_end(args);
  if(len < 0) throw(E_INVAL, NULL); /* XXX: Detail? */
  return (unsigned int)len;
}

gzFile *gzf_open(char *file, char *mode)
{
  gzFile *f = gzopen(file, mode);
  if(!f) throw(E_FACCESS, file);
  resource(f, (resource_handler)gzclose);
  return f;
}

unsigned int gzf_write(gzFile *f, void *buf, unsigned int max)
{
  unsigned int len = gzwrite(f, buf, max);
  if(!len && max != len) throw(E_FINVAL, f);
  return len;
}

unsigned int gzf_read(gzFile *f, void *buf, unsigned int max)
{
  int len = gzread(f, buf, max);
  if(len < 0) throw(E_FINVAL, f);
  return (unsigned int)len;
}

unsigned int gzf_putl(gzFile *f, char *buf)
{
  int len = gzputs(f, buf);
  if(len < 0) throw(E_FINVAL, f);
  return (unsigned int)len;
}

char *gzf_getl(gzFile *f, void *buf, unsigned int max)
{
  char *b = gzgets(f, buf, max);
  if(b == Z_NULL) throw(E_FINVAL, f);
  return b;
}
