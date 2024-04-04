#include "data.h"
#include "stdio.h"



void freeData(RespData* data) {
    switch (data->type) {
        case RESP_INTEGER:
            free(data);
            break;
        case RESP_BOOL:
            free(data);
            break;
        case RESP_NULL:
            free(data);
            break;
        case RESP_ARRAY:
            for (int i = 0; i < AS_ARR(data)->length; i++) {
                freeData(AS_ARR(data)->values[i]);
            }
            free(data);
            break;
        case RESP_BULK_STRING:
            free(AS_BLK_STR(data)->chars);
            free(data);
            break;
        case RESP_SIMPLE_STRING:
            free(AS_SIMPLE_STR(data));
            free(data);
            break;
        case RESP_SIMPLE_ERROR:
            free(AS_SIMPLE_ERROR(data));
            free(data);
            break;
    }
}
void printData(RespData* data) {
    printf("Not yet implemented");
}