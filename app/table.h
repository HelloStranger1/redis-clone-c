#ifndef redis_table_h
#define redis_table_h

#include <stdlib.h>
#include <stdio.h>
#include "common.h"

typedef uint32_t hashfunction (const char*, size_t);
typedef void cleanupfunction(void *);
typedef struct _hash_table hash_table;

hash_table* hash_table_create(uint32_t size, hashfunction* hf, cleanupfunction *cf);
void hash_table_free(hash_table *ht);
void hash_table_print(hash_table *ht);
bool hash_table_insert(hash_table *ht, const char *key, void *obj);
void *hash_table_get(hash_table *ht, const char *key);
void *hash_table_delete(hash_table *ht, const char *key);

#endif