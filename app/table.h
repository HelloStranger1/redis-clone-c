#ifndef redis_table_h
#define redis_table_h

#include <stdlib.h>
#include <stdio.h>
#include "utils.h"

typedef uint32_t hashfunction (const char*, size_t);
typedef void cleanupfunction(void *);
typedef struct _hash_table hash_table;

hash_table* hash_table_create(uint32_t size, hashfunction* hf, cleanupfunction *cf);
void hash_table_free(hash_table *ht);
void hash_table_print(hash_table *ht);

/**
 * returns the old key if exists, otherwise NULL
*/
void *hash_table_insert(hash_table *ht, const char *key, void *obj, long long expiry);

/*
* return NULL if key is expired, else the object (We asuume we can't store NULLS)
*/
void *hash_table_get(hash_table *ht, const char *key);
void *hash_table_delete(hash_table *ht, const char *key);

#endif