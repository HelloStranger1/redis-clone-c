#include "table.h"
#include <string.h>

typedef struct entry {
    char *key;
    void *object;
    struct entry *next;
} entry;

struct _hash_table {
    uint32_t size;
    hashfunction *hash;
    cleanupfunction *cleanup;
    entry **elements;
};


static size_t hash_table_index(hash_table *ht, const char *key) {
    size_t result = (ht->hash(key, strlen(key)) % ht->size);
}
hash_table* hash_table_create(uint32_t size, hashfunction* hf, cleanupfunction *cf) {
    hash_table *ht = malloc(sizeof(*ht));
    ht->size = size;
    ht->hash = hf;
    if (cf == NULL) {
        ht->cleanup = free;
    } else {
        ht->cleanup = cf;
    }
    ht->elements = calloc(sizeof(entry*), ht->size);
    return ht;
}

void hash_table_free(hash_table *ht) {
    for (uint32_t i = 0; i < ht->size; i++) {
        while (ht->elements[i]) {
            entry *tmp = ht->elements[i];
            ht->elements[i] = ht->elements[i]->next;
            free(tmp->key);
            ht->cleanup(tmp->object);
            free(tmp);
        }
    }

    free(ht->elements);
    free(ht);
}

void hash_table_print(hash_table *ht) {
    printf("Start table\n");
    
    for (uint32_t i = 0; i < ht->size; i++) {
        if (ht->elements[i] == NULL) {
            printf("\t%i\t---\n", i);
        } else {
            printf("\t%i\t\n", i);
            entry *tmp = ht->elements[i];
            while (!tmp) {
                printf("\"%s\"(%p) - ", tmp->key, tmp->object);
                tmp = tmp->next;
            }
            printf("\n");
        }
    }
    printf("End table\n");
}

bool hash_table_insert(hash_table *ht, const char *key, void *obj) {
    if (key == NULL || obj == NULL) return false;
    size_t index = hash_table_index(ht, key);

    if (hash_table_get(ht, key) != NULL) return false;

    // Create entry
    entry *e = malloc(sizeof(*e));
    e->object = obj;
    e->key = strdup(key);

    // Insert
    e->next = ht->elements[index];
    ht->elements[index] = e;
    return true;
}

void *hash_table_get(hash_table *ht, const char *key) {
    if (key == NULL) return false;
    size_t index = hash_table_index(ht, key);

    entry *tmp = ht->elements[index];
    while (tmp != NULL && strcmp(tmp->key, key) != 0) {
        tmp = tmp->next;
    }

    if (tmp == NULL) return NULL;
    return tmp->object;
}

void *hash_table_delete(hash_table *ht, const char *key) {
    if (key == NULL) return false;
    size_t index = hash_table_index(ht, key);

    entry *tmp = ht->elements[index];
    entry *prev = NULL;
    while (tmp != NULL && strcmp(tmp->key, key) != 0) {
        prev = tmp;
        tmp = tmp->next;
    }

    if (tmp == NULL) return NULL;
    if (prev == NULL) {
        // Deleting head
        ht->elements[index] = tmp->next;
    } else {
        prev->next = tmp->next;
    }

    void *result = tmp->object;
    free(tmp);
    return tmp->object;
}

