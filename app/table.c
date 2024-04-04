#include "table.h"


void initTable(Table* table) {
    table->size = 0;
    table->used = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
}
bool tableGet(Table* table, char* key, RespData** value);
bool tableSet(Table* table, char* key, RespData* value);
bool tableDelete(Table* table, char* key);