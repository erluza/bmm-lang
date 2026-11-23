#ifndef VALUE_H
#define VALUE_H

#include <stdint.h>
#include <stddef.h>

typedef enum { VAL_INT, VAL_STR, VAL_ARR, VAL_FN } ValType;

typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjArray ObjArray;

struct Obj {
    int type; /* ObjType */
    int marked;
    struct Obj *next;
};

typedef struct {
    int type; /* ValType */
    union {
        int32_t i;
        Obj *o;
    } as;
} Value;

#define IS_INT(v) ((v).type == VAL_INT)
#define IS_STR(v) ((v).type == VAL_STR)
#define IS_ARR(v) ((v).type == VAL_ARR)

Value val_int(int32_t i);
Value val_str(Obj *o);
Value val_arr(Obj *o);
Value val_null(void);

int val_truthy(Value v);
/* Returns a heap buffer owned by the caller. The caller must release it with
 * bmm_free(). */
char *val_to_cstr_repr(Value v);

/* Obj types */
typedef enum { OBJ_STRING, OBJ_ARRAY } ObjType;

struct ObjString {
    Obj base;
    size_t len;
    char *chars;
};

struct ObjArray {
    Obj base;
    size_t len;
    size_t cap;
    Value *items;
};

ObjString *new_string(const char *s, size_t len);
ObjArray *new_array(void);
void array_push(ObjArray *a, Value v);
Value array_pop(ObjArray *a);

#endif
