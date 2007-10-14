#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "hash_table.h"

/* Use 66% of the table, to keep clustering down, */
#define GET_SIZE(entries) (entries*3/2)
#define GET_ENTRIES(size) (size*2/3)

#define EMPTY   (void*)0x00
#define DELETED (void*)0x01

#define BASE_SZ 16

hash_t *hasht_init(unsigned int entries, void (*free_e)(void *),
                   unsigned long (*hash_e)(void *),
                   unsigned long (*hash_v)(void *),
                   int (*cmp_ee)(void *, void *),
                   int (*cmp_ev)(void *, void *))
{
  hash_t *table = malloc(sizeof(hash_t));
	unsigned int size;
	unsigned int shift;

  size = (entries > BASE_SZ) ? entries : BASE_SZ;
  size = GET_SIZE(size);

	/* Set the size to the nearest power of 2. */
	size -= 1;
	for(shift = 1; shift < sizeof(size); shift *= 2) size = size | size >> shift;
	size += 1;

  if(!table || !free_e || !hash_e || !hash_v || !cmp_ee || !cmp_ev) return NULL;

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

  for(; i < size; i++) if(v[i] != EMPTY && v[i] != DELETED) free_e(v[i]);

  free(table->table);
  free(table);
}

int hasht_rehash(hash_t *table, unsigned int factor)
{
  void **v = table->table;
  unsigned int i, size = table->size;

  if(table->iter != -1) return 0;

  for(i = 0; i < sizeof(factor); i++) {
    if((1 << i) == factor) {
      table->size *= factor;
      if(!(table->table = calloc(table->size, sizeof(void *)))) return -1;

      for(i = 0; i < size; i++)
        if(v[i] != EMPTY && v[i] != DELETED) hasht_add(table, v[i]);

      free(v);
      return 1;
    }
  }

  return -2;
}

void *hasht_add(hash_t *table, void *entry)
{
  int (*cmp_ee)(void*, void*) = table->cmp_ee;
  unsigned int mask = table->size - 1;
  unsigned int i = table->hash_e(entry) & mask;
  void **v = table->table;
  void *replaced;

  if(table->iter != -1) return NULL;
  if(table->used+1 > GET_ENTRIES(table->size)) {
    if(hasht_rehash(table, 2) != 1) return NULL;

    mask = table->size - 1;
    i = table->hash_e(entry) & mask;
    v = table->table;
  }

  while(1) {
    /* Insert a new value into the hash. */
    if(v[i] == EMPTY || v[i] == DELETED) {
      v[i] = entry;
      table->used++;
      return NULL;
    }
    /* Replace a duplicate value. */
    if(cmp_ee(entry, v[i]) == 0) {
      replaced = v[i];
      v[i] = entry;
      return replaced;
    }
    i = (i + 1) & mask;
  }

  return NULL; /* Not reached. */
}

void *hasht_get(hash_t *table, void *value)
{
  int (*cmp_ev)(void*, void*) = table->cmp_ev;
  unsigned int mask = table->size - 1;
  unsigned int i = table->hash_v(value) & mask;
  void **v = table->table;

  while(v[i] != EMPTY) {
    if(v[i] == DELETED) continue;
    if(cmp_ev(v[i], value) == 0) return v[i];
    i = (i + 1) & mask;
  }

  return NULL;
}

void *hasht_del(hash_t *table, void *value)
{
  int (*cmp_ev)(void*, void*) = table->cmp_ev;
  unsigned int mask = table->size - 1;
  unsigned int i = table->hash_v(value) & mask;
  void **v = table->table;
  void *removed;

  if(table->iter != -1) return NULL;

  while(v[i] != EMPTY) {
    if(v[i] == DELETED) continue;
    if(cmp_ev(v[i], value) == 0) {
      removed = v[i];

      /* Optimization: Don't add a join unless we need to. */
      if(v[(i + 1) & mask] == EMPTY) v[i] = EMPTY;
      else v[i] = DELETED;

      /* Update the table counts. */
      table->used--;
      if(++table->deleted >= table->size/5) hasht_rehash(table, 1);

      return removed;
    }
    i = (i + 1) & mask;
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

