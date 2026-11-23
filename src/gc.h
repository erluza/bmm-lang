#ifndef GC_H
#define GC_H

#include "value.h"

/* All heap allocation goes through these. */
void *bmm_alloc(size_t size);
void *bmm_realloc(void *p, size_t size);
void bmm_free(void *p);

typedef struct BmmVM BmmVM;

void gc_init(void);
void gc_shutdown(void);
Obj *gc_alloc_obj(int type, size_t size);
void gc_collect(BmmVM *vm);
void gc_add_root_value(Value *v); /* not used; VM roots traversed */

/* VM pointer for GC roots set at runtime */
void gc_set_vm(BmmVM *vm);

#endif
