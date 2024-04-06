#include "table.h"
#include <string.h>

typedef struct entry {
    char *key;
    void *object;
    struct entry *next;
    long long expiry;
} entry;

struct _hash_table {
    uint32_t size;
    uint64_t count;
    hashfunction *hash;
    cleanupfunction *cleanup;
    entry **elements;
};


static size_t hash_table_index(hash_table *ht, const char *key) {
    size_t result = (ht->hash(key, strlen(key)) % ht->size);
    return result;
}

static bool should_resize(hash_table *ht) {
    return (ht->count / ht->size) > 2;
}

static hash_table *hash_table_resize(hash_table *ht) {
    uint32_t old_size = ht->size;
    uint32_t new_size = old_size * 2;
    hash_table *new_table = hash_table_create(new_size, ht->hash, ht->cleanup);
    for (int i = 0; i < old_size; i++) {
        entry* tmp = ht->elements[i];
        while (!tmp) {
            entry* next = tmp->next;
            size_t index = hash_table_index(new_table, tmp->key);
            tmp->next = new_table->elements[index];
            new_table->elements[index] = tmp;
            new_table->count++;
            tmp = next;
        }
    }
    
    free(ht->elements);
    free(ht);
    return new_table;
}

hash_table* hash_table_create(uint32_t size, hashfunction* hf, cleanupfunction *cf) {
    hash_table *ht = malloc(sizeof(*ht));
    ht->size = size;
    ht->hash = hf;
    ht->count = 0;
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

void* hash_table_insert(hash_table *ht, const char *key, void *obj, long long expiry) {
    if (key == NULL || obj == NULL) return false;

    if (should_resize(ht)) {
        ht = hash_table_resize(ht);
    }

    size_t index = hash_table_index(ht, key);

    entry *tmp = ht->elements[index];
    while (tmp != NULL && strcmp(tmp->key, key)) {
        tmp = tmp->next;
    }
    if (tmp != NULL) {
        // We override
        void *previousObj = tmp->object;
        tmp->object = obj;
        return previousObj;
    }

    // Create entry
    entry *e = malloc(sizeof(*e));
    e->object = obj;
    e->key = strdup(key);
    e->expiry = expiry;

    // Insert
    e->next = ht->elements[index];
    ht->elements[index] = e;
    ht->count++;
    return NULL;
}

void *hash_table_get(hash_table *ht, const char *key) {
    if (key == NULL) return NULL;
    size_t index = hash_table_index(ht, key);

    entry *tmp = ht->elements[index];
    entry *prev = NULL;
    while (tmp != NULL && strcmp(tmp->key, key) != 0) {
        prev = tmp;
        tmp = tmp->next;
    }

    if (tmp == NULL) return NULL;
    if (tmp->expiry != -1 && current_timestamp() > tmp->expiry) {
        if (prev == NULL) {
            ht->elements[index] = tmp->next;
        } else {
            prev->next = tmp->next;
        }
        ht->cleanup(tmp->object);
        free(tmp);
        return NULL;
    }
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
    ht->count--;
    return result;
}

