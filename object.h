#pragma once
#include <stdint.h>


typedef enum {
    CHARACTER,
    SYMBOL,
    CONS,
    NUMBER,
    FIXNUM,
    POINTER,
    FN,
    NIL,
    BOOLEAN,
} imp_object_type;

typedef struct imp_object_struct *imp_object;

typedef struct imp_object_struct {
    imp_object_type type;
    union {
        int character;
        int64_t number;
        struct {
            char *name;
        } symbol;
        struct {
            imp_object head;
            imp_object tail;
        } cons;
        void *pointer;
        struct {
            void *entrypoint;
            int arity;
            imp_object closure[];
        } fn;
    } fields;
} imp_object_struct;

imp_object imp_symbol(const char *name);
imp_object imp_number(int64_t value);
imp_object imp_pointer(void *value);
imp_object imp_fixnum(int64_t value);
int        imp_is_fixnum(imp_object x);
int64_t    imp_cint(imp_object x);
imp_object_type imp_type_of(imp_object object);
imp_object imp_cons(imp_object head, imp_object tail);
imp_object imp_pair(imp_object x, imp_object y);
imp_object imp_first(imp_object list);
imp_object imp_rest(imp_object list);
imp_object imp_second(imp_object list);
imp_object imp_third(imp_object list);
imp_object imp_nth(imp_object list, int n);
int        imp_count(imp_object list);
int        imp_equals(imp_object x, imp_object y);
void       imp_print(imp_object object);
imp_object imp_read();
imp_object imp_lookup(imp_object haystack, imp_object needle);
imp_object imp_assoc(imp_object m, imp_object k, imp_object v);

extern const imp_object TRUE;
extern const imp_object FALSE;
extern const imp_object EMPTY_LIST;
