#ifndef redis_common_h
#define redis_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>

long long current_timestamp(void);
void die(const char *msg);

void rand_str(char *dest, size_t length);

#endif