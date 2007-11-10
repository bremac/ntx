#include <stdio.h>
#include <zlib.h>
#include "except.h"

/* XXX: Interpret errno, etc. */

FILE *raw_open(char *file, char *mode)
{
  FILE *f = fopen(file, mode);
  if(!f) throw(E_FACCESS, file);
  resource(f, fclose);
  return f;
}

unsigned int raw_getl(FILE *f, char *buf, unsigned int max)
{
  char *b = fgets(buf, max, f);
  if(!b && ferror(f) != 0) throw(E_FINVAL, f);
  return b;
}

void *alloc(unsigned int size)
{
  void *buf = malloc(size);
  if(!buf) throw(E_NOMEM, size);
  return buf;
}

void *ralloc(void *buf, unsigned int size)
{
  void *buf = realloc(buf, size);
  if(!buf) throw(E_NOMEM, size);
  return buf;
}

gzFile *gzf_open(char *file, char *mode)
{
  gzFile *f = gzopen(file, mode);
  if(!f) throw(E_FACCESS, file);
  resource(f, gzclose);
  return f;
}

unsigned int gzf_write(gzFile *f, void *buf, unsigned int max)
{
  unsigned int len = gzwrite(f, buf, len);
  if(!len && max != len) throw(E_FINVAL, f);
  return len;
}

unsigned int gzf_read(gzFile *f, void *buf, unsigned int max)
{
  int len = gzread(f, buf, len);
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
