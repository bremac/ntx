#ifndef EXC_IO__H
#define EXC_IO__H

#include <stdio.h>
#include <zlib.h>

FILE *raw_open(char *file, char *mode);
char* raw_getl(FILE *f, char *buf, unsigned int max);

void *alloc(unsigned int size);
void *ralloc(void *buf, unsigned int size);

char *strdupe(char *buf);
unsigned int seprintf(char *buf, unsigned int max, char *fmt, ...);

gzFile *gzf_open(char *file, char *mode);
unsigned int gzf_write(gzFile *f, void *buf, unsigned int max);
unsigned int gzf_read(gzFile *f, void *buf, unsigned int max);
unsigned int gzf_putl(gzFile *f, char *buf);
char *gzf_getl(gzFile *f, void *buf, unsigned int max);

#endif
