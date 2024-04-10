#include <stdio.h>
#include "parser.h"

int main()
{
    char buf[100] = "*1\r\n$4\r\nping\r\n";
    char* temp = buf;
    RespData* data = parse_resp_data(&temp);

    printf("first data: %s", data->as.arr->values[0]->as.blk_str->chars);
    printf("tried to compare, got: 1");
    return 0;
}
