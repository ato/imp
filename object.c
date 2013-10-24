/*-*- Mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*-*/
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"

static imp_object_struct END_OF_INPUT = {.type = CHARACTER, .fields = { .character = EOF, }};
static imp_object_struct LPAREN = {.type = CHARACTER, .fields = { .character = '('}};
static imp_object_struct RPAREN = {.type = CHARACTER, .fields = { .character = ')'}};

static imp_object_struct THE_TRUE = {.type = BOOLEAN};
static imp_object_struct THE_FALSE = {.type = BOOLEAN};
const imp_object TRUE = &THE_TRUE;
const imp_object FALSE = &THE_FALSE;
const imp_object EMPTY_LIST = NULL;

static const int MAX_NAME_LEN = 128;




imp_object imp_symbol(const char *name) {
    assert(name);
    char *symbol_name = malloc(strlen(name) + 1);
    imp_object symbol = malloc(sizeof(imp_object_struct));
    symbol->type = SYMBOL;
    strcpy(symbol_name, name);
    symbol->fields.symbol.name = symbol_name;
    return symbol;
}

imp_object imp_number(int64_t value) {
    imp_object number = malloc(sizeof(imp_object_struct));
    number->type = NUMBER;
    number->fields.number = value;
    return number;
}

imp_object imp_pointer(void *value) {
    imp_object pointer = malloc(sizeof(imp_object_struct));
    pointer->type = POINTER;
    pointer->fields.pointer = value;
    return pointer;
}

imp_object imp_fixnum(int64_t value) {
    return (imp_object) ((value << 1) | 1);
}

int imp_is_fixnum(imp_object x) {
    return ((uint64_t)x) & 1;
}

int64_t imp_cint(imp_object x) {
    if (imp_is_fixnum(x)) {
        return ((int64_t)x) >> 1;
    }
    assert(x->type == NUMBER);
    return x->fields.number;
}

imp_object_type imp_type_of(imp_object object) {
    if (object == NULL) {
        return NIL;
    } else if (imp_is_fixnum(object)) {
        return FIXNUM;
    }
    return object->type;
}

imp_object imp_cons(imp_object head, imp_object tail) {
    imp_object cell = malloc(sizeof(imp_object_struct));
    cell->type = CONS;
    cell->fields.cons.head = head;
    cell->fields.cons.tail = tail;
    return cell;
}

imp_object imp_pair(imp_object x, imp_object y) {
    return imp_cons(x, imp_cons(y, NULL));
}

imp_object imp_first(imp_object list) {
    assert(list->type = CONS);
    return list->fields.cons.head;
}

imp_object imp_rest(imp_object list) {
    assert(list->type = CONS);
    return list->fields.cons.tail;
}

imp_object imp_second(imp_object list) {
    return imp_first(imp_rest(list));
}

imp_object imp_third(imp_object list) {
    return imp_first(imp_rest(imp_rest(list)));
}

imp_object imp_nth(imp_object list, int n) {
    int i;
    for (i = 0; i < n && list; i++) {
        if (i == n) {
            return imp_first(list);            
        }
        list = imp_rest(list);
    }
    fprintf(stderr, "nth out of bound: %d > %d\n", n, i);
    abort();
}

int imp_count(imp_object list) {
    int n = 0;
    while (list != NULL) {
        n++;
        list = imp_rest(list);
    }
    return n;
}

int imp_equals(imp_object x, imp_object y) {
    if (x == y)
        return 1;
    if (x == NULL || y == NULL)
        return 0;
    if (imp_type_of(x) != imp_type_of(y))
        return 0;
    switch (imp_type_of(x)) {
    case FIXNUM: return x == y;
    case NUMBER: return x->fields.number == y->fields.number;
    case POINTER: return x->fields.pointer == y->fields.pointer;
    case CHARACTER: return x->fields.character == y->fields.character;
    case SYMBOL: return !strcmp(x->fields.symbol.name, y->fields.symbol.name);
    case CONS: return imp_equals(imp_first(x), imp_first(y)) && imp_equals(imp_rest(x), imp_rest(y));
    case NIL:
    case BOOLEAN:
    case FN: return x == y;
    }
}

static imp_object read_token() {
    int c;
    do {
        c = getchar();
        if (c == ';') {
            do {
                c = getchar();
            } while (c != '\n' && c != EOF);
        }
    } while (isspace(c));

    switch (c) {
    case EOF: return &END_OF_INPUT;
    case '(': return &LPAREN;
    case ')': return &RPAREN;
    default:
        {
            char name[MAX_NAME_LEN];
            char *pname = name;
            do {
                *pname++ = c;
                c = getchar();
            } while (c != EOF && !isspace(c) && c != '(' && c != ')');
            ungetc(c, stdin);
            *pname = '\0';
            if (isdigit(name[0])) {
                return imp_fixnum(strtoll(name, NULL, 10));
            } if (!strcmp("true", name)) {
                return TRUE;
            } else if (!strcmp("false", name)) {
                return FALSE;
            } else {
                return imp_symbol(name);
            }
        }
    }
}

void imp_print(imp_object object) {
    switch (imp_type_of(object)) {
    case NIL:
        printf("nil");
        return;
    case BOOLEAN:
        printf(object == FALSE ? "false" : "true");
        break;
    case CHARACTER:
        printf("\\%c", object->fields.character);
        break;
    case SYMBOL:
        printf("%s", object->fields.symbol.name);
        break;
    case FIXNUM:
    case NUMBER:
        printf("%ld", imp_cint(object));
        break;
    case POINTER:
        printf("#pointer %p", object->fields.pointer);
        break;
    case CONS:
        printf("(");
        imp_print(imp_first(object));
        for (imp_object tail = imp_rest(object); tail != NULL && tail->type == CONS; tail = imp_rest(tail)) {
            printf(" ");
            imp_print(imp_first(tail));
        }
        printf(")");
        break;
    case FN:
        printf("#fn {:entrypoint %p :arity %d}", object->fields.fn.entrypoint,
               object->fields.fn.arity);
        break;
    }
}

static imp_object imp_read_tail() {
    imp_object form = read_token();
    if (form == &END_OF_INPUT) {
        fprintf(stderr, "unexpected EOF\n");
        exit(1);
    } else if (form == &RPAREN) {
        return NULL;
    } else if (form == &LPAREN) {
        return imp_cons(imp_read_tail(), imp_read_tail());
    } else {
        return imp_cons(form, imp_read_tail());
    }
}

imp_object imp_read() {
    imp_object token = read_token();
    if (token == &END_OF_INPUT) {
        fprintf(stderr, "unexpected EOF\n");
        exit(1);
    } else if (token == &LPAREN) {
        return imp_read_tail();
    } else if (token == &RPAREN) {
        fprintf(stderr, "unexpected )\n");
        exit(1);
    } else {
        return token;
    }
}

imp_object imp_lookup(imp_object haystack, imp_object needle) {
    for (imp_object entry = haystack; entry != NULL; entry = imp_rest(entry)) {
        imp_object pair = imp_first(entry);
        imp_object key = imp_first(pair);
        imp_object value = imp_second(pair);
        if (imp_equals(key, needle)) {
            return value;
        }
    }
    return NULL;
}

imp_object imp_assoc(imp_object m, imp_object k, imp_object v) {
    return imp_cons(imp_pair(k, v), m);
}
