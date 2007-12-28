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

#include <stdlib.h>
#include <string.h>
#include "hash_table.h"

/* Use 66% of the table, to keep clustering down, */
#define GET_SIZE(entries) (entries*3/2)
#define GET_ENTRIES(size) (size*2/3)

/* Minimum size for a hash table. */
#define BASE_SZ 10

const int _deleted  = 0;        /* Symbolic address for DELETED. */
const void *EMPTY   = NULL;
const void *DELETED = &_deleted;


unsigned int _get_next_pwr_of_two(unsigned int number)
{
  unsigned int shift = 1, next = number - 1;

  for(; shift < sizeof(next); shift *= 2)
    next = next | next >> shift;

  return next + 1;
}

hash_t *hasht_init(unsigned int entries, void (*free_e)(void *),
                   unsigned long (*hash_e)(void *),
                   unsigned long (*hash_v)(void *),
                   int (*cmp_ee)(void *, void *),
                   int (*cmp_ev)(void *, void *))
{
  hash_t *table = malloc(sizeof(hash_t));
	unsigned int size;

  size = (entries > BASE_SZ) ? entries : BASE_SZ;
  size = _get_next_pwr_of_two(GET_SIZE(size));

  /* Check that resizing to 150% and rounding to the next power of  *
   * two doesn't overflow the unsigned integer.                     */
  if(size < entries)
    return NULL;

  /* Ensure that all necessary function pointers are available.     *
   * If free_e doesn't exist, on the other hand, it will be ignored *
   * when the table is free.                                        */
  if(!table || !hash_e || !hash_v || !cmp_ee || !cmp_ev)
    return NULL;

  table->size = size;
  table->table = calloc(size, sizeof(void *));

  if(!table->table) {
    free(table);
    return NULL;
  }

  table->deleted = 0;
  table->used    = 0;
  table->free    = free_e;
  table->hash_e  = hash_e;
  table->hash_v  = hash_v;
  table->cmp_ee  = cmp_ee;
  table->cmp_ev  = cmp_ev;
  table->iter    = -1;

  return table;
}

void hasht_free(hash_t *table)
{
  unsigned int size = table->size, i = 0;
  void **v = table->table;
  void (*free_e)(void *) = table->free;

  size = table->size;

  if(free_e != NULL)
    for(; i < size; i++)
      if(v[i] != EMPTY && v[i] != DELETED)
        free_e(v[i]);

  free(table->table);
  free(table);
}

int hasht_rehash(hash_t *table, unsigned int factor)
{
  void **v = table->table;
  unsigned int i, size = table->size;
  unsigned int multiple;

  if(table->iter != -1)
    return 0;

  multiple = _get_next_pwr_of_two(factor);

  /* Check that the next power of two won't overflow. */
  if(multiple < factor)
    return -1;

  table->size *= multiple;

  /* If we can't allocate a new table, restore the old state and return. */
  if(!(table->table = calloc(table->size, sizeof(void *)))) {
    table->size  = size;
    table->table = v;
    return -2;
  }

  /* Reset the deletions counter. */
  table->deleted = 0;

  for(i = 0; i < size; i++)
    if(v[i] != EMPTY && v[i] != DELETED)
      hasht_add(table, v[i]);

  free(v);
  return 1;
}

void *hasht_add(hash_t *table, void *entry)
{
  int (*cmp_ee)(void*, void*) = table->cmp_ee;
  unsigned int i, mask;
  void **v;

  /* Make sure we're not altering a table which we're iterating over. */
  if(table->iter != -1)
    return NULL;

  if(table->used+1 > GET_ENTRIES(table->size))
    if(hasht_rehash(table, 2) != 1)
      return NULL;

  mask = table->size - 1;
  v    = table->table;

  for(i = table->hash_e(entry) & mask;
      /* loop forever */ ;
      i = (i + 1) & mask)
  {
    /* Insert a new value into the hash. */
    if(v[i] == EMPTY || v[i] == DELETED) {
      v[i] = entry;
      table->used++;
      return NULL;
    }

    /* Replace a duplicate value. */
    if(cmp_ee(entry, v[i]) == 0) {
      void *replaced = v[i];
      v[i] = entry;
      return replaced;
    }
  }

  return NULL; /* Not reached. */
}

void *hasht_get(hash_t *table, void *value)
{
  int (*cmp_ev)(void*, void*) = table->cmp_ev;
  unsigned int i, mask = table->size - 1;
  void **v = table->table;

  for(i = table->hash_v(value) & mask;
      v[i] != EMPTY;
      i = (i + 1) & mask)
  {
    if(v[i] == DELETED)
      continue;

    if(cmp_ev(v[i], value) == 0)
      return v[i];
  }

  return NULL;
}

void *hasht_del(hash_t *table, void *value)
{
  int (*cmp_ev)(void*, void*) = table->cmp_ev;
  unsigned int i, mask = table->size - 1;
  void **v = table->table;

  /* Make sure we're not altering a table which we're iterating over. */
  if(table->iter != -1)
    return NULL;

  for(i = table->hash_v(value) & mask;
      v[i] != EMPTY;
      i = (i + 1) & mask)
  {
    if(v[i] == DELETED)
      continue;

    if(cmp_ev(v[i], value) == 0) {
      void *removed = v[i];

      /* Optimization: Don't add a DELETED unless we need to. */
      if(v[(i + 1) & mask] == EMPTY) {
        v[i] = (void*)EMPTY;
      } else {
        v[i] = (void*)DELETED;

        /* Rehash the table if we pass the following threshold. */
        if(++table->deleted >= table->size/5)
          hasht_rehash(table, 1);
      }

      /* Update the table used count. */
      table->used--;

      return removed;
    }
  }

  return NULL;
}

void *hasht_next(hash_t *table)
{
  void **v = table->table;
  unsigned int i, size = table->size;

  for(i = table->iter + 1; i < size; i++) {
    if(v[i] != EMPTY && v[i] != DELETED) {
      table->iter = i;
      return v[i];
    }
  }

  table->iter = -1;
  return NULL;
}

void hasht_done(hash_t *table)
{
  table->iter = -1;
}

