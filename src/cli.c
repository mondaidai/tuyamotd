#include "cli.h"

#include <argp.h>
#include <string.h>

static char doc[] =
    "Tuya IoT Daemon";

static char args_doc[] = "";

static struct argp_option options[] = {
    {"device-id",     'i', "DEVICE_ID",     0, "Device ID"},
    {"device-secret", 's', "DEVICE_SECRET", 0, "Device Secret"},
    {"product-id",    'p', "PRODUCT_ID",    0, "Product ID"},
    {"daemon",        'd', 0,               0, "Run as daemon"},
    {0}
};

struct arguments {
    app_args *app;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;

    switch (key)
    {
        case 'i':
            strncpy(arguments->app->device_id,
                    arg,
                    MAX_DEVICE_ID_LENGTH - 1);

            arguments->app->device_id[MAX_DEVICE_ID_LENGTH - 1] = '\0';
            break;

        case 's':
            strncpy(arguments->app->device_secret,
                    arg,
                    MAX_DEVICE_SECRET_LENGTH - 1);

            arguments->app->device_secret[MAX_DEVICE_SECRET_LENGTH - 1] = '\0';
            break;

        case 'p':
            strncpy(arguments->app->product_id,
                    arg,
                    MAX_PRODUCT_ID_LENGTH - 1);

            arguments->app->product_id[MAX_PRODUCT_ID_LENGTH - 1] = '\0';
            break;

        case 'd':
            arguments->app->is_daemon = true;
            break;

        case ARGP_KEY_END:

            if (strlen(arguments->app->device_id) == 0)
                argp_error(state, "Missing --device-id");

            if (strlen(arguments->app->device_secret) == 0)
                argp_error(state, "Missing --device-secret");

            if (strlen(arguments->app->product_id) == 0)
                argp_error(state, "Missing --product-id");

            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp = {
    options,
    parse_opt,
    args_doc,
    doc
};

int parse_args(int argc, char **argv, app_args *result)
{
    struct arguments arguments;

    memset(result, 0, sizeof(*result));

    result->is_daemon = false;

    arguments.app = result;

    return argp_parse(
        &argp,
        argc,
        argv,
        0,
        0,
        &arguments
    );
}