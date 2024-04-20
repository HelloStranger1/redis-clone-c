#ifndef redis_common_h
#define redis_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

long long current_timestamp(void);
void die(const char *msg);
void rand_str(char *dest, size_t length);


/**
 * Implementations taken from 
 * https://nachtimwald.com/2017/09/24/hex-encode-and-decode-in-c/
*/
char *bin2hex(const unsigned char *bin, size_t len);
size_t hexs2bin(const char *hex, unsigned char **out);
int hex2bin(const unsigned char *bin, size_t len);



#endif