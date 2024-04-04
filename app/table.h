#ifndef redis_table_h
#define redis_table_h

#include <stdlib.h>

#include "parser.h"
#include "data.h"

typedef struct {
    char* key;
    RespData* value;
} Entry;

typedef struct {
    int size; // Amount of Buckets
    int used; // Total elements stored
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, char* key, RespData** value);
bool tableSet(Table* table, char* key, RespData* value);
bool tableDelete(Table* table, char* key);
void tableAddAll(Table* from, Table* to);
#endif