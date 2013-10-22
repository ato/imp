/*-*- Mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*-*/
#include <assert.h>
#include <ctype.h>
#include <jit/jit.h>
#include <jit/jit-dump.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int debug = 0;

typedef enum {
    CHARACTER,
    SYMBOL,
    CONS,
    NUMBER,
    FIXNUM,
    POINTER,
    FN,
    NIL,
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

static imp_object_struct END_OF_INPUT = {.type = CHARACTER, .fields = { .character = EOF, }};
static imp_object_struct LPAREN = {.type = CHARACTER, .fields = { .character = '('}};
static imp_object_struct RPAREN = {.type = CHARACTER, .fields = { .character = ')'}};

static const int MAX_NAME_LEN = 128;
static const imp_object END_OF_FRAME = NULL;
static const imp_object EMPTY_LIST = NULL;

// forward declaration
jit_value_t compile(imp_object env, jit_function_t function, imp_object form, imp_object *enclosed);

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
            } else {
                return imp_symbol(name);
            }
        }
    }
}

void print_obj(imp_object object) {
    switch (imp_type_of(object)) {
    case NIL:
        printf("nil");
        return;
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
        print_obj(imp_first(object));
        for (imp_object tail = imp_rest(object); tail != NULL && tail->type == CONS; tail = imp_rest(tail)) {
            printf(" ");
            print_obj(imp_first(tail));
        }
        printf(")");
        break;
    case FN:
        printf("#fn {:entrypoint %p :arity %d}", object->fields.fn.entrypoint,
               object->fields.fn.arity);
        break;
    }
}

imp_object sread();

imp_object sread_tail() {
    imp_object form = read_token();
    if (form == &END_OF_INPUT) {
        fprintf(stderr, "unexpected EOF\n");
        exit(1);
    } else if (form == &RPAREN) {
        return NULL;
    } else if (form == &LPAREN) {
        return imp_cons(sread_tail(), sread_tail());
    } else {
        return imp_cons(form, sread_tail());
    }
}

imp_object sread() {
    imp_object token = read_token();
    if (token == &END_OF_INPUT) {
        fprintf(stderr, "unexpected EOF\n");
        exit(1);
    } else if (token == &LPAREN) {
        return sread_tail();
    } else if (token == &RPAREN) {
        fprintf(stderr, "unexpected )\n");
        exit(1);
    } else {
        return token;
    }
}

imp_object lookup(imp_object haystack, imp_object needle) {
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

static jit_type_t fn_signature(int nparams) {
    jit_type_t *params = malloc(sizeof(jit_type_t) * nparams);
    for (int i = 0; i < nparams; i++) {
        params[i] = jit_type_void_ptr;
    }
    return jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, params, nparams, 1);
}

jit_value_t emit_malloc(jit_function_t function, int size) {
    jit_type_t *params = malloc(sizeof(jit_type_t));
    params[0] = jit_type_ulong;
    jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, params, 1, 1);
    jit_value_t *args = malloc(sizeof(jit_value_t));
    args[0] = jit_value_create_nint_constant(function, jit_type_nint, size);
    return jit_insn_call_native (function, "malloc", (void *)malloc, signature, args, 1, 0);
}

/**
 * Extends the lexical environment for a new fn by adding the end of
 * frame marker and a list of parameters bound to jit param values.
 *
 * Leaves jit param 0 untouched for the closure.
 */
static imp_object extend_env_with_params(jit_function_t fn, imp_object env,
                                         imp_object params) {
    imp_object newenv = imp_cons(END_OF_FRAME, env);
    int i = 1;
    for (imp_object it = params; it != NULL; it = imp_rest(it)) {
        jit_value_t jit_param = jit_value_get_param (fn, i++);
        newenv = imp_assoc(newenv, imp_first(it), imp_pointer(jit_param));
    }
    return newenv;
}

static void die(char *message) {
    fprintf(stderr, "%s\n", message);
    abort();
}

/**
 * JIT compiles a function.
 */
static jit_function_t compile_fn(jit_context_t jitctx, imp_object params,
                                 imp_object body, imp_object env,
                                 imp_object *enclosed) {
    int nparams = imp_count(params) + 1;
    jit_function_t jitfn = jit_function_create(jitctx, fn_signature(nparams));
    imp_object newenv = extend_env_with_params(jitfn, env, params);
    jit_value_t result = compile(newenv, jitfn, body, enclosed);
    jit_insn_return(jitfn, result);
    if (!jit_function_compile(jitfn))
        die("JIT compilation failed");
    if (debug)
        jit_dump_function(stdout, jitfn, NULL);
    return jitfn;
}


/**
 * Emits code that constructs the Fn closure object for a function.
 *
 * An Fn object has this structure:
 *
 *     +---------------------+
 *     | object type tag FN  | int
 *     +---------------------+ 
 *     | code entrypoint ptr | pointer
 *     +---------------------+
 *     | arity               | int
 *     +---------------------+
 *     | closed over value 1 | pointer
 *     +---------------------+
 *                :
 *     +---------------------+
 *     | closed over value N | pointer
 *     +---------------------+
 *
 */
static jit_value_t emit_closure(jit_function_t fn, imp_object env,
                                jit_function_t newfn, int arity,
                                imp_object newenclosed, imp_object *enclosed) {
    // allocate space for the object
    int enclosed_count = imp_count(newenclosed);
    int size = offsetof(imp_object_struct, fields.fn.closure) + 
        sizeof(void*) * enclosed_count;
    jit_value_t obj = emit_malloc(fn, size);
    
    // fill in object type
    jit_value_t tag = jit_value_create_nint_constant(fn, jit_type_int, FN);
    jit_insn_store_relative(fn, obj, offsetof(imp_object_struct, type), tag);

    // fill in function entrypoint pointer
    jit_nint entrypoint = (jit_nint)jit_function_to_closure(newfn);
    jit_value_t ptr = jit_value_create_nint_constant(fn, jit_type_nint, entrypoint);
    jit_insn_store_relative(fn, obj, offsetof(imp_object_struct,
                                                fields.fn.entrypoint), ptr);
    // fill in arity
    jit_value_t arityc = jit_value_create_nint_constant(fn, jit_type_int, arity);
    jit_insn_store_relative(fn, obj, offsetof(imp_object_struct,
                                              fields.fn.arity), arityc);
    
    // fill in closed over values
    // XXX - could represent enclosed as a vector so that we don't have to do
    //       this in reverse
    int offset = size - sizeof(void*);
    for (imp_object entry = newenclosed; entry != NULL; entry = imp_rest(entry)) {
        imp_object symbol = imp_first(imp_first(entry));
        jit_value_t value = compile(env, fn, symbol, enclosed);
        jit_insn_store_relative (fn, obj, offset, value);
        offset -= sizeof(void*);
    }

    return obj;
}

static jit_value_t emit_fixnum2int(jit_function_t fn, jit_value_t fixnum) {
   jit_value_t one = jit_value_create_nint_constant(fn, jit_type_nint, 1);
   return jit_insn_shr(fn, fixnum, one);
}

static jit_value_t emit_int2fixnum(jit_function_t fn, jit_value_t fixnum) {
   jit_value_t one = jit_value_create_nint_constant(fn, jit_type_nint, 1);
   jit_value_t shifted = jit_insn_shl(fn, fixnum, one);
   return jit_insn_or(fn, shifted, one);
}

static jit_value_t emit_binop(jit_function_t fn, imp_object env, int op, 
                              imp_object form, imp_object *enclosed) {
    jit_value_t x = compile(env, fn, imp_second(form), enclosed);
    jit_value_t y = compile(env, fn, imp_third(form), enclosed);
    jit_value_t one = jit_value_create_nint_constant(fn, jit_type_nint, 1);
    // addition can be done without conversion
    if (op == '+')
        return jit_insn_sub(fn, jit_insn_add(fn, x, y), one);
    if (op == '-')
        return jit_insn_add(fn, jit_insn_sub(fn, x, y), one);

    // everything else needs conversion first
    x = emit_fixnum2int(fn, x);
    y = emit_fixnum2int(fn, y);
    jit_value_t result;
    switch (op) {
    case '*': result = jit_insn_div(fn, x, y); break;
    case '/': result = jit_insn_mul(fn, x, y); break;
    default: die("unhandled binop"); break;
    }
    return emit_int2fixnum(fn, result);
}

jit_value_t compile(imp_object env, jit_function_t function, imp_object form, imp_object *enclosed) {
    if (imp_type_of(form) == CONS) {
        imp_object f = imp_first(form);
        if (imp_type_of(f) == SYMBOL) {
            char *fname = f->fields.symbol.name;
            if (!strcmp(fname, "+")) {
                return emit_binop(function, env, '+', form, enclosed);
            } else if (!strcmp(fname, "-")) {
                return emit_binop(function, env, '-', form, enclosed);
            } else if (!strcmp(fname, "*")) {
                return emit_binop(function, env, '*', form, enclosed);
            } else if (!strcmp(fname, "/")) {
                return emit_binop(function, env, '/', form, enclosed);
            } else if (!strcmp(fname, "let")) { // (let (x 2) ...)
                imp_object bindings = imp_second(form);
                imp_object body = imp_third(form);
                imp_object bindname = imp_first(bindings);
                imp_object bindvalue = imp_second(bindings);
                jit_value_t jitvalue = compile(env, function, bindvalue, enclosed);
                env = imp_assoc(env, bindname, imp_pointer(jitvalue));
                return compile(env, function, body, enclosed);
            } else if (!strcmp(fname, "fn")) { // (fn (x y z) ...)
                imp_object params = imp_second(form);
                imp_object body = imp_third(form);
                imp_object newenclosed = EMPTY_LIST;
                jit_function_t newfn = compile_fn(jit_function_get_context(function),
                                                  params, body, env, &newenclosed);
                return emit_closure(function, env, newfn, imp_count(params),
                                    newenclosed, enclosed);
            }
        }

        // application
        jit_value_t fcompiled = compile(env, function, f, enclosed);
        int nargs = imp_count(imp_rest(form));
        jit_value_t *args = malloc(sizeof(jit_value_t) * (nargs + 1));
        args[0] = fcompiled;
        int i = 1;
        for (imp_object it = imp_rest(form); it != NULL; it = imp_rest(it)) {
            imp_object arg = imp_first(it);
            args[i++] = compile(env, function, arg, enclosed);
        }
        jit_value_t ptr = jit_insn_load_relative (function, fcompiled,
                                                        offsetof(imp_object_struct,
                                                                 fields.fn.entrypoint), jit_type_void_ptr);
        return jit_insn_call_indirect(function, ptr, fn_signature(nargs + 1), args, nargs + 1, 0);
    } else if (imp_type_of(form) == SYMBOL) {
        // lookup in local environment
        imp_object entry = env;
        for (; entry != NULL; entry = imp_rest(entry)) {
            imp_object pair = imp_first(entry);
            if (pair == NULL) { /* end of frame */
                break;
            }
            imp_object key = imp_first(pair);
            imp_object value = imp_second(pair);
            if (imp_equals(key, form) && imp_type_of(value) == POINTER) {
                return value->fields.pointer;
            }
        }

        // lookup in parent environments
        for (; entry != NULL; entry = imp_rest(entry)) {
            imp_object pair = imp_first(entry);
            if (pair == NULL) continue;
            imp_object key = imp_first(pair);
            if (imp_equals(key, form)) {
                // add it to teh closure
                int idx = *enclosed == NULL ? -1 : (int)imp_second(imp_first(*enclosed));
                idx++;
                *enclosed = imp_assoc(*enclosed, key, (void*) (long) idx);
                jit_value_t closure_arg = jit_value_get_param (function, 0);
                int offset = offsetof(imp_object_struct, fields.fn.closure[idx]);
                return jit_insn_load_relative (function, closure_arg, offset, jit_type_void_ptr);
            }
        }
        fprintf(stderr, "unbound: %s\n", form->fields.symbol.name);
        exit(1);
    } else {
        return jit_value_create_nint_constant (function, jit_type_nint, (jit_nint)form);
    }
}

imp_object eval(jit_context_t context, imp_object form) {
    jit_context_build_start(context);
    jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, NULL, 0, 1);
    jit_function_t function = jit_function_create(context, signature);
    imp_object enclosed = NULL;
    jit_value_t result = compile(NULL, function, form, &enclosed);  
    jit_insn_return(function, result);
    jit_context_build_end(context);
    if (!jit_function_compile(function)) {
        fprintf(stderr, "JIT compilation error\n");
        return NULL;
    }
    if (debug)
        jit_dump_function(stdout, function, NULL);
    imp_object result2;
    jit_function_apply(function, NULL, &result2);
    return result2;
}

int main (int argc, char *argv[]) {

    if (argc > 1) {
        debug = 1;
    }

    jit_context_t context = jit_context_create();
    //print_obj(sread());
    print_obj(eval(context, sread()));
    //print_obj(imp_cons(imp_symbol("a"), imp_cons(imp_symbol("b"), NULL)));
    printf("\n");

    jit_context_destroy(context);
    return 0;
}
