#include "gc.h"
#include "vm.h"
#include <stdlib.h>
#include <string.h>

static Obj *gc_objects = NULL;
static BmmVM *gc_vm = NULL;
static Obj *gc_pending = NULL;

void gc_set_vm(BmmVM *vm) { gc_vm = vm; }

void *bmm_alloc(size_t size) {
    return malloc(size == 0 ? 1 : size);
}

void *bmm_realloc(void *p, size_t size) {
    return realloc(p, size == 0 ? 1 : size);
}

void bmm_free(void *p) {
    free(p);
}

void gc_init(void) {
    gc_objects = NULL;
}

void gc_shutdown(void) {
    /* free all */
    Obj *o = gc_objects;
    while (o) {
        Obj *n = o->next;
        if (o->type == OBJ_STRING) {
            ObjString *s = (ObjString*)o;
            bmm_free(s->chars);
        } else if (o->type == OBJ_ARRAY) {
            ObjArray *a = (ObjArray*)o;
            bmm_free(a->items);
        }
        bmm_free(o);
        o = n;
    }
    gc_objects = NULL;
}

Obj *gc_alloc_obj(int type, size_t size) {
    Obj *o = bmm_alloc(size);
    memset(o, 0, size);
    o->type = type;
    o->marked = 0;
    o->next = gc_objects;
    gc_objects = o;
    /* protect from collection */
    gc_pending = o;
    /* threshold collection */
    static int count = 0;
    count++;
    if (count >= BMM_GC_THRESHOLD) {
        count = 0;
        if (gc_vm) gc_collect(gc_vm);
    }
    gc_pending = NULL;
    return o;
}

static void mark_obj(Obj *o);
static void mark_value(Value v) {
    if (v.type == VAL_STR || v.type == VAL_ARR) {
        mark_obj(v.as.o);
    }
}

static void mark_obj(Obj *o) {
    if (!o || o->marked) return;
    o->marked = 1;
    if (o->type == OBJ_ARRAY) {
        ObjArray *a = (ObjArray*)o;
        for (size_t i = 0; i < a->len; i++) mark_value(a->items[i]);
    }
    /* strings no refs */
}

void gc_collect(BmmVM *vm) {
    if (!vm) return;
    /* unmark all */
    for (Obj *o = gc_objects; o; o = o->next) o->marked = 0;
    /* mark roots */
    for (size_t i = 0; i < vm->sp; i++) mark_value(vm->stack[i]);
    for (size_t f = 0; f < vm->fp; f++) {
        Frame *fr = &vm->frames[f];
        if (fr->locals) {
            for (int i = 0; i < fr->func->nlocals; i++) mark_value(fr->locals[i]);
        }
    }
    for (size_t i = 0; i < vm->nglobals; i++) mark_value(vm->globals[i]);
    if (vm->argv_arr) mark_obj((Obj*)vm->argv_arr);
    if (gc_pending) mark_obj(gc_pending);
    /* mark consts in all funcs */
    for (size_t fi = 0; fi < vm->nfuncs; fi++) {
        Func *fn = vm->funcs[fi];
        if (fn) {
            for (size_t c = 0; c < fn->nconsts; c++) mark_value(fn->consts[c]);
        }
    }
    /* sweep */
    Obj **pp = &gc_objects;
    while (*pp) {
        Obj *o = *pp;
        if (!o->marked) {
            *pp = o->next;
            if (o->type == OBJ_STRING) {
                ObjString *s = (ObjString*)o;
                bmm_free(s->chars);
            } else if (o->type == OBJ_ARRAY) {
                ObjArray *a = (ObjArray*)o;
                bmm_free(a->items);
            }
            bmm_free(o);
        } else {
            pp = &o->next;
        }
    }
}
