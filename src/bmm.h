#ifndef BMM_H
#define BMM_H

#include "bmm_config.h"
#include "pal/pal.h"

typedef struct {
    const char *name;
} BmmValue; /* placeholder */

/* Public small API */
int bmm_run_file(const char *path, int argc, char **argv, int max_steps);
int bmm_run_source(const char *src, const char *filename, int argc, char **argv, int max_steps);
void bmm_repl(void);

#endif
