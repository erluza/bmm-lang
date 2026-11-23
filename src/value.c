#include "value.h"
#include "gc.h"
#include <string.h>

Value val_int(int32_t i) { Value v; v.type = VAL_INT; v.as.i = i; return v; }
Value val_str(Obj *o) { Value v; v.type = VAL_STR; v.as.o = o; return v; }
Value val_arr(Obj *o) { Value v; v.type = VAL_ARR; v.as.o = o; return v; }
Value val_null(void) { Value v; v.type = VAL_INT; v.as.i = 0; return v; }

int val_truthy(Value v) {
    if (v.type == VAL_INT) return v.as.i != 0;
    return 1;
}

char *val_to_cstr_repr(Value v) {
    /* returns malloc'd string */
    if (v.type == VAL_INT) {
        char *b = bmm_alloc(32);
        int neg = v.as.i < 0;
        unsigned int uvi = neg ? (unsigned int)(-(v.as.i + 1)) + 1u : (unsigned int)v.as.i;
        char tmp[32];
        int ti = 0;
        if (uvi == 0) tmp[ti++] = '0';
        else while (uvi > 0) { tmp[ti++] = '0' + (uvi % 10); uvi /= 10; }
        int bi = 0;
        if (neg) b[bi++] = '-';
        while (ti > 0) b[bi++] = tmp[--ti];
        b[bi] = '\0';
        return b;
    }
    if (v.type == VAL_STR) {
        ObjString *s = (ObjString*)v.as.o;
        char *b = bmm_alloc(s->len + 1);
        memcpy(b, s->chars, s->len);
        b[s->len] = '\0';
        return b;
    }
    if (v.type == VAL_ARR) {
        ObjArray *a = (ObjArray*)v.as.o;
        /* build [..] */
        size_t cap = 64;
        char *b = bmm_alloc(cap);
        size_t len = 0;
        b[len++] = '[';
        for (size_t i = 0; i < a->len; i++) {
            if (i) { 
                if (len + 2 >= cap) { cap *= 2; b = bmm_realloc(b, cap); }
                b[len++] = ','; b[len++] = ' ';
            }
            char *r = val_to_cstr_repr(a->items[i]);
            size_t rl = strlen(r);
            while (len + rl + 4 >= cap) { cap *= 2; b = bmm_realloc(b, cap); }
            memcpy(b + len, r, rl);
            len += rl;
            bmm_free(r);
        }
        if (len + 2 >= cap) { cap *= 2; b = bmm_realloc(b, cap); }
        b[len++] = ']';
        b[len] = '\0';
        return b;
    }
    char *b = bmm_alloc(8);
    strcpy(b, "?");
    return b;
}

ObjString *new_string(const char *s, size_t len) {
    ObjString *o = (ObjString*)gc_alloc_obj(OBJ_STRING, sizeof(ObjString));
    o->len = len;
    o->chars = bmm_alloc(len + 1);
    if (len && s) memcpy(o->chars, s, len);
    o->chars[len] = '\0';
    return o;
}

ObjArray *new_array(void) {
    ObjArray *o = (ObjArray*)gc_alloc_obj(OBJ_ARRAY, sizeof(ObjArray));
    o->len = 0;
    o->cap = 8;
    o->items = bmm_alloc(o->cap * sizeof(Value));
    return o;
}

void array_push(ObjArray *a, Value v) {
    if (a->len >= a->cap) {
        a->cap *= 2;
        a->items = bmm_realloc(a->items, a->cap * sizeof(Value));
    }
    a->items[a->len++] = v;
}

Value array_pop(ObjArray *a) {
    if (a->len == 0) return val_int(0);
    return a->items[--a->len];
}
