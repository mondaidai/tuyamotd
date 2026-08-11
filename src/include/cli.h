#ifndef CLI_H
#define CLI_H

#include <stdbool.h>

#define MAX_DEVICE_ID_LENGTH 64
#define MAX_DEVICE_SECRET_LENGTH 64
#define MAX_PRODUCT_ID_LENGTH 64

typedef struct {
    char device_id[MAX_DEVICE_ID_LENGTH];
    char device_secret[MAX_DEVICE_SECRET_LENGTH];    
    char product_id[MAX_PRODUCT_ID_LENGTH];    
    bool is_daemon;
} app_args;

int parse_args(int argc, char **argv, app_args *result);

#endif