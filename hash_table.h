#ifndef NAIVE_HASH__H
#define NAIVE_HASH__H

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
