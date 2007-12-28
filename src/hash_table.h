/* 
 * Extensible hash table written in C. Should be suitable as
 * a basis for whatever overlays/wrapper functions which you
 * choose to add onto it.
 *
 * Copyright (c) 2006, 2007 Brendan MacDonell
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef HASH_TABLE__H
#define HASH_TABLE__H

typedef struct {
	unsigned int size, used, deleted;
  long int iter;
  void **table;

  unsigned long (*hash_e)(void *);
  unsigned long (*hash_v)(void *);
  int (*cmp_ee)(void *, void *);
  int (*cmp_ev)(void *, void *);
  void (*free)(void *);
} hash_t;

unsigned long hasht_hash(char *key, unsigned long length, unsigned long init);

hash_t *hasht_init(unsigned int entries, void (*free_e)(void *),
                   unsigned long (*hash_e)(void *),
                   unsigned long (*hash_v)(void *),
                   int (*cmp_ee)(void *, void *),
                   int (*cmp_ev)(void *, void *));
int hasht_rehash(hash_t *table, unsigned int factor);
void hasht_free(hash_t *table);

void *hasht_add(hash_t *table, void *entry);
void *hasht_get(hash_t *table, void *value);
void *hasht_del(hash_t *table, void *value);

void *hasht_next(hash_t *table);
void hasht_done(hash_t *table);

#endif
