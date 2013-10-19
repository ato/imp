#include <assert.h>
#include <ctype.h>
#include <jit/jit.h>
#include <jit/jit-dump.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  CHARACTER,
  SYMBOL,
  CONS,
  NUMBER,
  FIXNUM,
  POINTER,
  FN,
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
      void *jit_fn;
      int nargs;
    } fn;
  } fields;
} imp_object_struct;

static imp_object_struct END_OF_INPUT = {.type = CHARACTER, .fields = { .character = EOF, }};
static imp_object_struct LPAREN = {.type = CHARACTER, .fields = { .character = '('}};
static imp_object_struct RPAREN = {.type = CHARACTER, .fields = { .character = ')'}};

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

imp_object imp_fn(void *jit_fn, int nargs) {
  imp_object fn = malloc(sizeof(imp_object_struct));
  fn->type = FN;
  fn->fields.fn.jit_fn = jit_fn;
  fn->fields.fn.nargs = nargs;
  return fn;
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
  if (imp_is_fixnum(object)) {
    return FIXNUM;
  }
  return object->type;
}

imp_object imp_cons(imp_object head, imp_object tail) {
  assert(head);
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
  if (object == NULL) {
    printf("nil");
    return;
  }
  switch (imp_type_of(object)) {
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
    printf("#fn %p", object);
    break;
  }
}

imp_object sread();

imp_object sread_tail() {
  imp_object form = sread();
  if (form != NULL) {
    return imp_cons(form, sread_tail());
  } else {
    return NULL;
  }
}

imp_object sread() {
  imp_object token = read_token();
  if (token == &LPAREN) {
    return imp_cons(sread(), sread_tail());
  } else if (token == &RPAREN) {
    return NULL;
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

jit_value_t compile(imp_object env, jit_function_t function, imp_object form) {
  if (imp_type_of(form) == CONS) {
    imp_object f = imp_first(form);
    if (imp_type_of(f) == SYMBOL) {
      char *fname = f->fields.symbol.name;
      if (!strcmp(fname, "+")) {
        jit_value_t x = compile(env, function, imp_second(form));
        jit_value_t y = compile(env, function, imp_third(form));
        jit_value_t tmp = jit_insn_add(function, x, y);
        jit_value_t one = jit_value_create_nint_constant(function, jit_type_nint, 1);
        return jit_insn_sub(function, tmp, one);
      } else if (!strcmp(fname, "-")) {
        jit_value_t x = compile(env, function, imp_second(form));
        jit_value_t y = compile(env, function, imp_third(form));
        jit_value_t tmp = jit_insn_sub(function, x, y);
        jit_value_t one = jit_value_create_nint_constant(function, jit_type_nint, 1);
        return jit_insn_add(function, tmp, one);
      } else if (!strcmp(fname, "*")) {
        jit_value_t x = compile(env, function, imp_second(form));
        jit_value_t y = compile(env, function, imp_third(form));
        jit_value_t one = jit_value_create_nint_constant(function, jit_type_nint, 1);
        jit_value_t x1 = jit_insn_shr(function, x, one);
        jit_value_t y1 = jit_insn_shr(function, y, one);
        jit_value_t tmp1 = jit_insn_mul(function, x1, y1);
        jit_value_t tmp = jit_insn_shl(function, tmp1, one);
        return jit_insn_add(function, tmp, one);
      } else if (!strcmp(fname, "/")) {
        jit_value_t x = compile(env, function, imp_second(form));
        jit_value_t y = compile(env, function, imp_third(form));
        jit_value_t one = jit_value_create_nint_constant(function, jit_type_nint, 1);
        jit_value_t x1 = jit_insn_shr(function, x, one);
        jit_value_t y1 = jit_insn_shr(function, y, one);
        jit_value_t tmp1 = jit_insn_div(function, x1, y1);
        jit_value_t tmp = jit_insn_shl(function, tmp1, one);
        return jit_insn_add(function, tmp, one);
      } else if (!strcmp(fname, "let")) { // (let (x 2) ...)
        imp_object bindings = imp_second(form);
        imp_object body = imp_third(form);
        imp_object bindname = imp_first(bindings);
        imp_object bindvalue = imp_second(bindings);
        jit_value_t jitvalue = compile(env, function, bindvalue);
        env = imp_assoc(env, bindname, imp_pointer(jitvalue));
        return compile(env, function, body);
      } else if (!strcmp(fname, "fn")) { // (fn (x y z) ...)
        imp_object params = imp_second(form);
        imp_object body = imp_third(form);
        int nparams = imp_count(params);
        jit_type_t signature = fn_signature(nparams);
        jit_function_t fn = jit_function_create(jit_function_get_context(function), signature);

        imp_object newenv = NULL;
        for (imp_object it = params, i = 0; it != NULL; it = imp_rest(it)) {
          jit_value_t jit_param = jit_value_get_param (fn, i++);
          newenv = imp_assoc(newenv, imp_first(it), imp_pointer(jit_param));
        }

        jit_value_t result = compile(newenv, fn, body);
        jit_insn_return(fn, result);
        if (!jit_function_compile(fn)) {
          fprintf(stderr, "JIT compilation error\n");
          exit(1);
        }
        jit_dump_function(stdout, fn, NULL);
        imp_object ifn = imp_fn(jit_function_to_vtable_pointer(fn), nparams);
        return jit_value_create_nint_constant (function, jit_type_nint, (jit_nint)ifn);
      }
    }
    jit_value_t fcompiled = compile(env, function, f);
    int nargs = imp_count(imp_rest(form));
    jit_value_t *args = malloc(sizeof(jit_value_t) * nargs);
    int i = 0;
    for (imp_object it = imp_rest(form); it != NULL; it = imp_rest(it)) {
      imp_object arg = imp_first(it);
      args[i++] = compile(env, function, arg);
    }
    int offset = offsetof(imp_object_struct, fields.fn.jit_fn);
    jit_value_t vtableptr = jit_insn_load_relative (function, fcompiled, offset, jit_type_void_ptr);
    return jit_insn_call_indirect_vtable(function, vtableptr, fn_signature(nargs), args, nargs, 0);
  } else if (imp_type_of(form) == SYMBOL) {
    imp_object value = lookup(env, form);
    if (value != NULL && imp_type_of(value) == POINTER) {
      return value->fields.pointer;
    } else {
      fprintf(stderr, "unbound: %s\n", form->fields.symbol.name);
      exit(1);
    }
  } else {
    return jit_value_create_nint_constant (function, jit_type_nint, (jit_nint)form);
  }
}

imp_object eval(jit_context_t context, imp_object form) {
  jit_context_build_start(context);
  jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, NULL, 0, 1);
  jit_function_t function = jit_function_create(context, signature);
  jit_value_t result = compile(NULL, function, form);  
  jit_insn_return(function, result);
  jit_context_build_end(context);
  if (!jit_function_compile(function)) {
    fprintf(stderr, "JIT compilation error\n");
    return NULL;
  }
  jit_dump_function(stdout, function, NULL);
  imp_object result2;
  jit_function_apply(function, NULL, &result2);
  return result2;
}

int main() {
  jit_context_t context = jit_context_create();
  print_obj(eval(context, sread()));
  //print_obj(imp_cons(imp_symbol("a"), imp_cons(imp_symbol("b"), NULL)));
  printf("\n");

  jit_context_destroy(context);
  return 0;
}
