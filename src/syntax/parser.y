/*%glr-parser*/
%define parse.error verbose
%define api.pure true
%param {OpiScanner *yyscanner}
%locations

%{
#define YYDEBUG 1

#include "opium/opium.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

extern int
yylex();

extern int
yylex_destroy(void);

extern
FILE* yyin;

static
OpiAst *g_result;

static
char g_filename[PATH_MAX];

static char**
g_errorptr;

static OpiLocation*
location(void *locp);

void
yyerror(void *locp, OpiScanner *yyscanner, const char *what);

struct binds {
  struct cod_strvec vars;
  struct cod_ptrvec vals;
};

static inline struct binds*
binds_new(void)
{
  struct binds *binds = malloc(sizeof(struct binds));
  cod_strvec_init(&binds->vars);
  cod_ptrvec_init(&binds->vals);
  return binds;
}

static inline void
binds_delete(struct binds *binds)
{
  cod_strvec_destroy(&binds->vars);
  cod_ptrvec_destroy(&binds->vals, NULL);
  free(binds);
}

static inline void
binds_push(struct binds *binds, const char *var, OpiAst *val)
{
  cod_strvec_push(&binds->vars, var);
  cod_ptrvec_push(&binds->vals, val, NULL);
}

typedef cod_vec(int) start_token_t;
extern
start_token_t opi_start_token;
%}

%union {
  opi_t opi;
  OpiAst *ast;
  long double num;
  char c;
  char *str;
  struct binds *binds;
  struct cod_strvec *strvec;
  struct cod_ptrvec *ptrvec;
  struct {
    struct cod_ptrvec *ptrvec;
    int iref;
  } args;

  OpiAstPattern *pattern;
  struct {
    struct cod_strvec fields;
    cod_vec(OpiAstPattern*) patterns;
  } patterns;
  cod_vec(OpiAstPattern*) pattvec;

  cod_vec(char) s;
  struct {
    cod_vec(char) s;
    cod_vec(OpiAst*) p;
  } fmt;

  struct {
    struct cod_strvec names;
    cod_vec(OpiAst*) body;
  } table;
}

%destructor { opi_drop($$); } <opi>
%destructor { opi_ast_delete($$); } <ast>
%destructor { free($$); } <str>
%destructor { binds_delete($$); } <binds>
%destructor { cod_strvec_destroy($$); free($$); } <strvec>
%destructor {
  for (size_t i = 0; i < $$->size; ++i)
    opi_ast_delete($$->data[i]);
  cod_ptrvec_destroy($$, NULL);
  free($$);
} <ptrvec>
%destructor {
  cod_vec_destroy($$.s);
  for (size_t i = 0; i < $$.p.len; ++i)
    opi_ast_delete($$.p.data[i]);
  cod_vec_destroy($$.p);
} <fmt>
%destructor {
  cod_strvec_destroy(&$$.names);
  for (size_t i = 0; i < $$.body.len; ++i)
    opi_ast_delete($$.body.data[i]);
  cod_vec_destroy($$.body);
} <table>

//
// Tokens
//
%nonassoc DUMMYCOL DUMMYVAR

%right '=' RARROW LARROW FN

%nonassoc STRUCT TRAIT IMPL FOR

%token<c>   CHAR
%token<num> NUMBER
%token<str> SYMBOL TYPE SHELL
%token<opi> CONST
%token<str> S

%nonassoc LET REC IN AND
%type<ast> use
%nonassoc BEG END

%right BRK

%right ';'

%right LAZY ASSERT

%left IF UNLESS WHEN
%precedence THEN
%precedence ELSE
%type<ast> if unless when
%token LOAD
%token DCOL
%token MODULE
%token USE AS
%nonassoc RETURN
%token DOTDOT

%token FMT_START
%token FMT_END

%token START_FILE
%token START_REPL

%token FLEX_ERROR

//
// Patterns
//
%type<ast> Atom
%type<ast> Form
%type<ast> Expr Stmnt

%type<binds> binds recbinds
%type<pattvec> param
%type<args> arg arg_aux
%type<ast> block block_stmnt block_stmnt_only block_expr
%type<ptrvec> block_aux
%type<ast> fn_aux vafn_aux anyfn_aux
%type<ast> def_aux vadef_aux anydef_aux
%type<ast> lambda valambda anylambda
%type<str> Symbol Type SymbolOrType
%type<pattern> pattern atomicPattern
%type<patterns> patterns
%type<pattvec> list_pattern_aux
%type<strvec> fields fields_aux
%type<ast> string shell qr sr
%type<fmt> str_aux shell_aux qr_aux
%type<fmt> fmt
%type<ast> table_binds table
%type<table> table_aux cond_table_aux
%type<table> ctor_aux

%nonassoc '@'

%right OR
%left RPIPE
%right '$'
%right SCOR
%right SCAND
%nonassoc IS ISNOT EQ EQUAL NUMLT NUMGT NUMLE NUMGE NUMEQ NUMNE
%nonassoc<str> ISOF
%right ':' PLUSPLUS
%left '+' '-'
%left '*' '/' '%'
%right '^'
%right '.'
%nonassoc NOT
%left TABLEREF

%left UMINUS

%start entry
%%

entry
  : START_FILE block { @$; g_result = $2; }
  | START_REPL block_expr {
    g_result = $2;
  }
  | START_REPL { g_result = NULL; }
  | error { g_result = NULL; }
;

ctor_aux
  : SYMBOL '=' Expr {
    cod_strvec_init(&$$.names);
    cod_vec_init($$.body);
    cod_strvec_push(&$$.names, $1); free($1);
    cod_vec_push($$.body, $3);
  }
  | ctor_aux ',' SYMBOL '=' Expr {
    $$ = $1;
    cod_strvec_push(&$$.names, $3); free($3);
    cod_vec_push($$.body, $5);
  }
;

Atom
  : NUMBER { $$ = opi_ast_const(opi_num_new($1)); }
  | Symbol { $$ = opi_ast_var($1); free($1); }
  | CONST { $$ = opi_ast_const($1); }
  | string
  | shell
  | qr
  | sr
  | Type '{' ctor_aux '}' {
    $$ = opi_ast_ctor($1, $3.names.data, $3.body.data, $3.body.len, NULL);
    cod_strvec_destroy(&$3.names);
    cod_vec_destroy($3.body);
    free($1);
  }
  | Type '{' ctor_aux ',' '}' {
    $$ = opi_ast_ctor($1, $3.names.data, $3.body.data, $3.body.len, NULL);
    cod_strvec_destroy(&$3.names);
    cod_vec_destroy($3.body);
    free($1);
  }
  | Type '{' ctor_aux ',' DOTDOT Expr '}' {
    $$ = opi_ast_ctor($1, $3.names.data, $3.body.data, $3.body.len, $6);
    cod_strvec_destroy(&$3.names);
    cod_vec_destroy($3.body);
    free($1);
  }
  | '(' Type ')' { $$ = opi_ast_var($2); free($2); }
  | '(' Expr ')' { $$ = $2; }
  | '[' arg_aux ']' {
    opi_assert($2.iref < 0);
    $$ = opi_ast_apply(opi_ast_var("List"), (OpiAst**)$2.ptrvec->data, $2.ptrvec->size);
    $$->apply.loc = location(&@$);
    cod_ptrvec_destroy($2.ptrvec, NULL);
    free($2.ptrvec);
  }
  | '[' '|' arg_aux '|' ']' {
    opi_assert($3.iref < 0);
    $$ = opi_ast_apply(opi_ast_var("Array"), (OpiAst**)$3.ptrvec->data, $3.ptrvec->size);
    $$->apply.loc = location(&@$);
    cod_ptrvec_destroy($3.ptrvec, NULL);
    free($3.ptrvec);
  }
  | '[' ']' { $$ = opi_ast_const(opi_nil); }
  | Atom TABLEREF SYMBOL {
    OpiAst *p[] = { $1, opi_ast_const(opi_symbol($3)) };
    $$ = opi_ast_apply(opi_ast_var("#"), p, 2);
    $$->apply.loc = location(&@$);
    free($3);
  }
  | Atom TABLEREF TYPE {
    OpiAst *p[] = { $1, opi_ast_const(opi_symbol($3)) };
    $$ = opi_ast_apply(opi_ast_var("#"), p, 2);
    $$->apply.loc = location(&@$);
    free($3);
  }
  | Atom TABLEREF '(' Expr ')' {
    OpiAst *p[] = { $1, $4 };
    $$ = opi_ast_apply(opi_ast_var("#"), p, 2);
    $$->apply.loc = location(&@$);
  }
;

SymbolOrType: Symbol | Type;
Symbol
  : SYMBOL
  | Type '.' SYMBOL {
    size_t len = strlen($1) + 1 + strlen($3);
    $$ = malloc(len + 1);
    sprintf($$, "%s.%s", $1, $3);
    free($1);
    free($3);
  }
;

Type
  : TYPE
  | Type '.' TYPE {
    size_t len = strlen($1) + 1 + strlen($3);
    $$ = malloc(len + 1);
    sprintf($$, "%s.%s", $1, $3);
    free($1);
    free($3);
  }
;

Form
  : Atom
  | Atom arg {
    $$ = opi_ast_apply($1, (OpiAst**)$2.ptrvec->data, $2.ptrvec->size);
    $$->apply.loc = location(&@$);
    if ($2.iref >= 0) {
      char retnam[] = "$ ret";
      char sttnam[] = "$ stt";
      char *fields[] = { "car", "cdr" };
      OpiAstPattern *pats[] = {
        opi_ast_pattern_new_ident(retnam),
        opi_ast_pattern_new_ident(sttnam)
      };
      OpiAstPattern *pat = opi_ast_pattern_new_unpack("Cons", pats, fields, 2, NULL);
      OpiAst *ref = $2.ptrvec->data[$2.iref];
      opi_assert(ref->tag == OPI_AST_VAR);
      OpiAst *body[] = {
        opi_ast_match(pat, $$, NULL, NULL),
        opi_ast_setref(ref->var, opi_ast_var(sttnam)),
        opi_ast_var("$ ret"),
      };
      $$ = opi_ast_block(body, 3);
    }
    cod_ptrvec_destroy($2.ptrvec, NULL);
    free($2.ptrvec);
  }
  | Type arg {
    opi_assert($2.iref < 0);
    $$ = opi_ast_apply(opi_ast_var($1), (OpiAst**)$2.ptrvec->data, $2.ptrvec->size);
    $$->apply.loc = location(&@$);
    cod_ptrvec_destroy($2.ptrvec, NULL);
    free($1);
    free($2.ptrvec);
  }
;

Stmnt: if | unless | when | use
  | LET REC recbinds IN Expr {
    $$ = opi_ast_fix($3->vars.data, (OpiAst**)$3->vals.data, $3->vars.size, $5);
    binds_delete($3);
  }
  | LET binds IN Expr {
    $$ = opi_ast_let($2->vars.data, (OpiAst**)$2->vals.data, $2->vars.size, $4);
    binds_delete($2);
  }
  | LET pattern '=' Expr IN Expr {
    OpiAst *body[] = { opi_ast_match($2, $4, NULL, NULL), $6 };
    $$ = opi_ast_block(body, 2);
  }
  | IF LET pattern '=' Expr THEN Expr ELSE Expr { $$ = opi_ast_match($3, $5, $7, $9); }
  | UNLESS LET pattern '=' Expr THEN Expr ELSE Expr { $$ = opi_ast_match($3, $5, $9, $7); }
  | RETURN Expr {
    $$ = opi_ast_return($2);
  }
  | BEG Expr END { $$ = $2; }
;

Expr
  : Form
  | Stmnt
  | Expr ';' Expr {
    OpiAst *body[] = { $1, $3 };
    $$ = opi_ast_block(body, 2);
  }
  | Expr ';' %prec DUMMYCOL { $$ = $1; }
  | '(' ')' {
    $$ = opi_ast_apply(opi_ast_var("()"), NULL, 0);
    $$->apply.loc = location(&@$);
  }
  | anylambda
  | LAZY Expr {
    OpiAst *fn = opi_ast_fn(NULL, 0, $2);
    $$ = opi_ast_apply(opi_ast_var("lazy"), &fn, 1);
    $$->apply.loc = location(&@$);
  }
  | ASSERT Expr {
    opi_t err = opi_undefined(opi_symbol("assertion-failed"));
    $$ = opi_ast_if($2, opi_ast_const(opi_nil),
        opi_ast_return(opi_ast_const(err)));
  }
  | Expr IS Expr {
    OpiAst *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("is"), p, 2);
    $$->apply.loc = location(&@$);
  }
  | Expr IS Type { $$ = opi_ast_isof($1, $3); free($3); }
  | Expr EQ Expr {
    OpiAst *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("eq"), p, 2);
    $$->apply.loc = location(&@$);
  }
  | Expr EQUAL Expr {
    OpiAst *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("equal"), p, 2);
    $$->apply.loc = location(&@$);
  }
  | Expr ISNOT Expr {
    OpiAst *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("is"), p, 2);
    $$->apply.loc = location(&@$);
    $$ = opi_ast_apply(opi_ast_var("not"), &$$, 1);
    $$->apply.loc = location(&@$);
  }
  | Expr NOT EQ Expr {
    OpiAst *p[] = { $1, $4 };
    $$ = opi_ast_apply(opi_ast_var("eq"), p, 2);
    $$->apply.loc = location(&@$);
    $$ = opi_ast_apply(opi_ast_var("not"), &$$, 1);
    $$->apply.loc = location(&@$);
  }
  | Expr NOT EQUAL Expr {
    OpiAst *p[] = { $1, $4 };
    $$ = opi_ast_apply(opi_ast_var("equal"), p, 2);
    $$->apply.loc = location(&@$);
    $$ = opi_ast_apply(opi_ast_var("not"), &$$, 1);
    $$->apply.loc = location(&@$);
  }
  | NOT Expr {
    OpiAst *x = $2;
    $$ = opi_ast_apply(opi_ast_var("not"), &x, 1);
    $$->apply.loc = location(&@$);
  }
  | Expr NUMLT Expr { $$ = opi_ast_binop(OPI_OPC_LT, $1, $3); }
  | Expr NUMGT Expr { $$ = opi_ast_binop(OPI_OPC_GT, $1, $3); }
  | Expr NUMLE Expr { $$ = opi_ast_binop(OPI_OPC_LE, $1, $3); }
  | Expr NUMGE Expr { $$ = opi_ast_binop(OPI_OPC_GE, $1, $3); }
  | Expr NUMEQ Expr { $$ = opi_ast_binop(OPI_OPC_NUMEQ, $1, $3); }
  | Expr NUMNE Expr { $$ = opi_ast_binop(OPI_OPC_NUMNE, $1, $3); }
  | Expr SCAND Expr { $$ = opi_ast_and($1, $3); }
  | Expr SCOR Expr { $$ = opi_ast_or($1, $3); }
  | '-' Expr %prec UMINUS {
    if ($2->tag == OPI_AST_CONST && $2->cnst->type == opi_num_type) {
      long double x = opi_num_get_value($2->cnst);
      opi_as($2->cnst, OpiNum).val = -x;
      $$ = $2;
    } else {
      OpiAst *p[] = { opi_ast_const(opi_num_new(0)), $2 };
      $$ = opi_ast_apply(opi_ast_var("-"), p, 2);
      $$->apply.loc = location(&@$);
    }
  }
  | '+' Expr %prec UMINUS { $$ = $2; }
  | Expr '+' Expr { $$ = opi_ast_binop(OPI_OPC_ADD, $1, $3); }
  | Expr '-' Expr { $$ = opi_ast_binop(OPI_OPC_SUB, $1, $3); }
  | Expr '*' Expr { $$ = opi_ast_binop(OPI_OPC_MUL, $1, $3); }
  | Expr '/' Expr { $$ = opi_ast_binop(OPI_OPC_DIV, $1, $3); }
  | Expr '%' Expr { $$ = opi_ast_binop(OPI_OPC_FMOD, $1, $3); }
  | Expr '^' Expr {
    OpiAst *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("^"), p, 2);
    $$->apply.loc = location(&@$);
  }
  | Expr ':' Expr { $$ = opi_ast_binop(OPI_OPC_CONS, $1, $3); }
  | Expr '.' Expr {
    OpiAst *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("."), p, 2);
    $$->apply.loc = location(&@$);
  }
  | Expr PLUSPLUS Expr {
    OpiAst *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("++"), p, 2);
    $$->apply.loc = location(&@$);
  }
  | Expr '$' Expr {
    if ($1->tag == OPI_AST_APPLY) {
      opi_ast_append_arg($1, $3);
      $$ = $1;
    } else {
      $$ = opi_ast_apply($1, &$3, 1);
    }
  }
  | Expr RPIPE Expr {
    if ($3->tag == OPI_AST_APPLY) {
      opi_ast_append_arg($3, $1);
      $$ = $3;
    } else {
      $$ = opi_ast_apply($3, &$1, 1);
    }
  }
  | Type '$' Expr { $$ = opi_ast_apply(opi_ast_var($1), &$3, 1); free($1); }
  | Expr OR Expr { $$ = opi_ast_eor($1, $3, " "); }
  | Expr OR SYMBOL RARROW Expr %prec THEN { $$ = opi_ast_eor($1, $5, $3); free($3); }
  | table
  /*| Symbol LARROW Expr {*/
    /*$$ = opi_ast_setref($1, $3);*/
    /*free($1);*/
  /*}*/
  | LET pattern LARROW Expr IN Expr {
    OpiAst *fn = opi_ast_fn_new_with_patterns(&$2, 1, $6);
    if ($4->tag == OPI_AST_APPLY) {
      opi_ast_append_arg($4, fn);
      $$ = $4;
    } else {
      $$ = opi_ast_apply($4, &fn, 1);
    }
  }
;

if
  : IF Expr THEN Expr {
    $$ = opi_ast_if($2, $4, opi_ast_const(opi_nil));
  }
  | IF Expr THEN Expr ELSE Expr {
    $$ = opi_ast_if($2, $4, $6);
  }
;

unless
  : UNLESS Expr THEN Expr {
    $$ = opi_ast_if($2, opi_ast_const(opi_nil), $4);
  }
  | UNLESS Expr THEN Expr ELSE Expr {
    $$ = opi_ast_if($2, $6, $4);
  }
;

when
  /*
   * when <tets-expr>
   * then <then-expr>
   */
  : WHEN Expr THEN Expr {
    $$ = opi_ast_when($2, "", $4, "", opi_ast_const(opi_nil));
  }
  /*
   * when <tets-expr>
   * then <then-bind> -> <then-expr>
   */
  | WHEN Expr THEN SYMBOL RARROW Expr {
    $$ = opi_ast_when($2, $4, $6, "", opi_ast_const(opi_nil));
    free($4);
  }
  /*
   * when <tets-expr>
   * then <then-expr>
   * else <else-expr>
   */
  | WHEN Expr THEN Expr ELSE Expr {
    $$ = opi_ast_when($2, "", $4, "", $6);
  }
  /*
   * when <tets-expr>
   * then <then-bind> -> <then-expr>
   * else <else-expr>
   */
  | WHEN Expr THEN SYMBOL RARROW Expr ELSE Expr {
    $$ = opi_ast_when($2, $4, $6, "", $8);
    free($4);
  }
  /*
   * when <tets-expr>
   * then <then-expr>
   * else <else-bind> -> <else-expr>
   */
  | WHEN Expr THEN Expr ELSE SYMBOL RARROW Expr {
    $$ = opi_ast_when($2, "", $4, $6, $8);
    free($6);
  }
  /*
   * when <tets-expr>
   * then <then-bind> -> <then-expr>
   * else <else-bind> -> <else-expr>
   */
  | WHEN Expr THEN SYMBOL RARROW Expr ELSE SYMBOL RARROW Expr {
    $$ = opi_ast_when($2, $4, $6, $8, $10);
    free($4);
    free($8);
  }
;

use
  : USE Symbol AS SYMBOL IN Expr {
    $$ = opi_ast_use($2, $4);
    free($2);
    free($4);
    OpiAst *body[] = { $$, $6 };
    $$ = opi_ast_block(body, 2);
  }
  | USE Type AS TYPE IN Expr {
    $$ = opi_ast_use($2, $4);
    free($2);
    free($4);
    OpiAst *body[] = { $$, $6 };
    $$ = opi_ast_block(body, 2);
  }
  | USE SymbolOrType IN Expr {
    char *p = strrchr($2, '.');
    opi_assert(p);
    $$ = opi_ast_use($2, p + 1);
    free($2);
    OpiAst *body[] = { $$, $4 };
    $$ = opi_ast_block(body, 2);
  }
  | USE Type '.' '*' IN Expr {
    size_t len = strlen($2) + 1;
    char buf[len + 1];
    sprintf(buf, "%s.", $2);
    $$ = opi_ast_use(buf, "*");
    free($2);
    OpiAst *body[] = { $$, $6 };
    $$ = opi_ast_block(body, 2);
  }
;

atomicPattern
  : SYMBOL { $$ = opi_ast_pattern_new_ident($1); free($1); }
  | '(' pattern ')' { $$ = $2; }
  | Type '{' '}' {
    $$ = opi_ast_pattern_new_unpack($1, NULL, NULL, 0, NULL);
    free($1);
  }
  | SYMBOL '@' Type '{' '}' {
    $$ = opi_ast_pattern_new_unpack($3, NULL, NULL, 0, $1);
    free($3);
    free($1);
  }
  | Type '{' patterns SYMBOL '}' {
    cod_strvec_push(&$3.fields, $4);
    cod_vec_push($3.patterns, opi_ast_pattern_new_ident($4));
    free($4);
    $$ = opi_ast_pattern_new_unpack($1, $3.patterns.data, $3.fields.data, $3.fields.size, NULL);
    free($1);
    cod_strvec_destroy(&$3.fields);
    cod_vec_destroy($3.patterns);
  }
  | SYMBOL '@' Type '{' patterns SYMBOL '}' {
    cod_strvec_push(&$5.fields, $6);
    cod_vec_push($5.patterns, opi_ast_pattern_new_ident($6));
    free($6);
    $$ = opi_ast_pattern_new_unpack($3, $5.patterns.data, $5.fields.data, $5.fields.size, $1);
    free($3);
    cod_strvec_destroy(&$5.fields);
    cod_vec_destroy($5.patterns);
    free($1);
  }
  | Type '{' patterns SYMBOL '=' pattern '}' {
    cod_strvec_push(&$3.fields, $4);
    cod_vec_push($3.patterns, $6);
    free($4);
    $$ = opi_ast_pattern_new_unpack($1, $3.patterns.data, $3.fields.data, $3.fields.size, NULL);
    cod_strvec_destroy(&$3.fields);
    cod_vec_destroy($3.patterns);
    free($1);
  }
  | SYMBOL '@' Type '{' patterns SYMBOL '=' pattern '}' {
    cod_strvec_push(&$5.fields, $6);
    cod_vec_push($5.patterns, $8);
    free($6);
    $$ = opi_ast_pattern_new_unpack($3, $5.patterns.data, $5.fields.data, $5.fields.size, $1);
    cod_strvec_destroy(&$5.fields);
    cod_vec_destroy($5.patterns);
    free($3);
    free($1);
  }
  | '[' list_pattern_aux ']' {
    OpiAstPattern *pat = opi_ast_pattern_new_ident("");
    for (int i = $2.len - 1; i >= 0; --i) {
      char *fields[] = { "car", "cdr" };
      OpiAstPattern *pats[] = { $2.data[i], pat };
      pat = opi_ast_pattern_new_unpack("Cons", pats, fields, 2, NULL);
    }
    $$ = pat;
    cod_vec_destroy($2);
  }
  | '[' ']' { $$ = opi_ast_pattern_new_unpack("Nil", NULL, NULL, 0, NULL); }
;
pattern
  : atomicPattern
  | pattern ':' pattern {
    char *fields[] = { "car", "cdr" };
    OpiAstPattern *pats[] = { $1, $3 };
    $$ = opi_ast_pattern_new_unpack("Cons", pats, fields, 2, NULL);
  }
  | SYMBOL '@' pattern ':' pattern {
    char *fields[] = { "car", "cdr" };
    OpiAstPattern *pats[] = { $3, $5 };
    $$ = opi_ast_pattern_new_unpack("Cons", pats, fields, 2, $1);
    free($1);
  }
  | Type list_pattern_aux {
    char *fields[$2.len];
    char buf[0x10];
    for (size_t i = 0; i < $2.len; ++i) {
      sprintf(buf, "#%zu", i);
      fields[i] = strdup(buf);
    }
    $$ = opi_ast_pattern_new_unpack($1, $2.data, fields, $2.len, NULL);
    for (size_t i = 0; i < $2.len; ++i)
      free(fields[i]);
    free($1);
    cod_vec_destroy($2);
  }
  | SYMBOL '@' Type list_pattern_aux {
    char *fields[$4.len];
    char buf[0x10];
    for (size_t i = 0; i < $4.len; ++i) {
      sprintf(buf, "#%zu", i);
      fields[i] = strdup(buf);
    }
    $$ = opi_ast_pattern_new_unpack($3, $4.data, fields, $4.len, $1);
    for (size_t i = 0; i < $4.len; ++i)
      free(fields[i]);
    free($3);
    cod_vec_destroy($4);
    free($1);
  }
  | Type { $$ = opi_ast_pattern_new_unpack($1, NULL, NULL, 0, NULL); free($1); }
  | SYMBOL '@' Type { $$ = opi_ast_pattern_new_unpack($3, NULL, NULL, 0, $1); free($3); free($1); }
;

list_pattern_aux
  : atomicPattern { cod_vec_init($$); cod_vec_push($$, $1); }
  | list_pattern_aux atomicPattern { $$ = $1; cod_vec_push($$, $2); }
;

patterns
  : {
    cod_strvec_init(&$$.fields);
    cod_vec_init($$.patterns);
  }
  | patterns SYMBOL '=' pattern ',' {
    $$ = $1;
    cod_strvec_push(&$$.fields, $2);
    cod_vec_push($$.patterns, $4);
    free($2);
  }
  | patterns SYMBOL ',' {
    $$ = $1;
    cod_strvec_push(&$$.fields, $2);
    cod_vec_push($$.patterns, opi_ast_pattern_new_ident($2));
    free($2);
  }
;

anylambda: lambda | valambda;

lambda: FN fn_aux { $$ = $2; };

valambda
  : FN vafn_aux { $$ = $2; }
;

param
  : '(' ')' { cod_vec_init($$); }
  | list_pattern_aux { $$ = $1; }
;

arg
  : '(' ')' {
    $$.ptrvec = malloc(sizeof(struct cod_ptrvec));
    $$.iref = -1;
    cod_ptrvec_init($$.ptrvec);
  }
  | arg_aux
  | FN anyfn_aux {
    $$.ptrvec = malloc(sizeof(struct cod_ptrvec));
    $$.iref = -1;
    cod_ptrvec_init($$.ptrvec);
    cod_ptrvec_push($$.ptrvec, $2, NULL);
  }
  | arg_aux FN anyfn_aux { $$ = $1; cod_ptrvec_push($$.ptrvec, $3, NULL); }
;

arg_aux
  : Atom {
    $$.ptrvec = malloc(sizeof(struct cod_ptrvec));
    $$.iref = -1;
    cod_ptrvec_init($$.ptrvec);
    cod_ptrvec_push($$.ptrvec, $1, NULL);
  }
  | '&' Symbol {
    $$.ptrvec = malloc(sizeof(struct cod_ptrvec));
    $$.iref = 0;
    cod_ptrvec_init($$.ptrvec);
    cod_ptrvec_push($$.ptrvec, opi_ast_var($2), NULL);
    free($2);
  }
  | arg_aux Atom {
    $$ = $1;
    cod_ptrvec_push($$.ptrvec, $2, NULL);
  }
  | arg_aux '&' Symbol {
    $$ = $1;
    opi_assert($$.iref < 0);
    $$.iref = $$.ptrvec->size;
    cod_ptrvec_push($$.ptrvec, opi_ast_var($3), NULL);
    free($3);
  }
;

block
  : { $$ = opi_ast_block(NULL, 0); }
  | block_aux {
    $$ = opi_ast_block((OpiAst**)$1->data, $1->size);
    cod_ptrvec_destroy($1, NULL);
    free($1);
  }
  | block_aux BRK {
    $$ = opi_ast_block((OpiAst**)$1->data, $1->size);
    cod_ptrvec_destroy($1, NULL);
    free($1);
  }
;

block_aux
  : block_expr {
    $$ = malloc(sizeof(struct cod_ptrvec));
    cod_ptrvec_init($$);
    cod_ptrvec_push($$, $1, NULL);
  }
  | block_aux BRK block_expr {
    $$ = $1;
    cod_ptrvec_push($$, $3, NULL);
  }
  | block_aux block_stmnt {
    $$ = $1;
    cod_ptrvec_push($$, $2, NULL);
  }
  /*| block_aux error {*/
    /*// WTF !?!?!?*/
    /*$$ = $1;*/
    /*opi_error = 1;*/
  /*}*/
;

block_expr
  : block_stmnt_only
  | Expr
;

block_stmnt: Stmnt | block_stmnt_only;

block_stmnt_only
  : LET REC recbinds {
    $$ = opi_ast_fix($3->vars.data, (OpiAst**)$3->vals.data, $3->vars.size, NULL);
    binds_delete($3);
  }
  | LET binds {
    $$ = opi_ast_let($2->vars.data, (OpiAst**)$2->vals.data, $2->vars.size, NULL);
    binds_delete($2);
  }
  | LET pattern '=' Expr { $$ = opi_ast_match($2, $4, NULL, NULL); }
  | LOAD string {
    opi_assert($2->tag == OPI_AST_CONST);
    $$ = opi_ast_load(OPI_STR($2->cnst)->str);
    opi_ast_delete($2);
  }
  | MODULE TYPE '=' block END {
    $$ = $4;
    char prefix[strlen($2) + 2];
    sprintf(prefix, "%s.", $2);
    opi_ast_block_set_namespace($$, prefix);
    opi_ast_block_set_drop($$, FALSE);
    free($2);
  }
  | STRUCT TYPE '{' fields '}' {
    $$ = opi_ast_struct($2, $4->data, $4->size);
    free($2);
    cod_strvec_destroy($4);
    free($4);
  }
  | TRAIT TYPE '=' cond_table_aux END {
    OpiAst *list_args[$4.names.size];
    OpiAst *impls[$4.names.size + 1];
    int n_impl = 0;
    for (size_t i = 0; i < $4.names.size; ++i) {
      if ($4.body.data[i] == NULL)
        continue;
      OpiAst *key = opi_ast_const(opi_symbol($4.names.data[i]));
      OpiAst *val = opi_ast_var($4.names.data[i]);
      list_args[n_impl] = opi_ast_binop(OPI_OPC_CONS, key, val);
      impls[n_impl] = $4.body.data[i];
      n_impl++;
    }

    OpiAst *list = opi_ast_apply(opi_ast_var("List"), list_args, n_impl);

    impls[n_impl] = list;
    OpiAst *block = opi_ast_block(impls, n_impl + 1);

    OpiAst *table = opi_ast_apply(opi_ast_var("Table"), &block, 1);

    char *table_name = " trait table ";
    OpiAst *methods[n_impl];
    for (size_t i = 0; i < $4.names.size; ++i) {
      if ($4.body.data[i] == NULL) {
        methods[i] = NULL;
      } else {
        OpiAst *p[] = {
            opi_ast_var(table_name),
            opi_ast_const(opi_symbol($4.names.data[i]))
        };
        methods[i] = opi_ast_apply(opi_ast_var("#"), p, 2);
      }
    }
    cod_vec_destroy($4.body);

    $$ = opi_ast_trait($2, $4.names.data, methods, $4.names.size);
    $$->trait.build = opi_ast_let(&table_name, &table, 1, NULL);

    free($2);
    cod_strvec_destroy(&$4.names);
  }
  | IMPL Type FOR Type '=' table_aux END {
    OpiAst *list_args[$6.names.size];
    for (size_t i = 0; i < $6.names.size; ++i) {
      OpiAst *key = opi_ast_const(opi_symbol($6.names.data[i]));
      OpiAst *val = opi_ast_var($6.names.data[i]);
      list_args[i] = opi_ast_binop(OPI_OPC_CONS, key, val);
    }

    OpiAst *list = opi_ast_apply(opi_ast_var("List"), list_args, $6.names.size);

    cod_vec_push($6.body, list);
    OpiAst *block = opi_ast_block($6.body.data, $6.body.len);
    cod_vec_destroy($6.body);

    OpiAst *table = opi_ast_apply(opi_ast_var("Table"), &block, 1);

    char *table_name = " trait table ";
    OpiAst *methods[$6.names.size];
    for (size_t i = 0; i < $6.names.size; ++i) {
      OpiAst *p[] = {
          opi_ast_var(table_name),
          opi_ast_const(opi_symbol($6.names.data[i]))
      };
      methods[i] = opi_ast_apply(opi_ast_var("#"), p, 2);
    }

    OpiAst *my_body[] = {
      opi_ast_let(&table_name, &table, 1, NULL),
      opi_ast_impl($2, $4, $6.names.data, methods, $6.names.size)
    };
    $$ = opi_ast_block(my_body, 2);
    opi_ast_block_set_drop($$, FALSE);

    free($2);
    free($4);
    cod_strvec_destroy(&$6.names);
  }
  | USE Symbol AS SYMBOL {
    $$ = opi_ast_use($2, $4);
    free($2);
    free($4);
  }
  | USE Type AS TYPE {
    $$ = opi_ast_use($2, $4);
    free($2);
    free($4);
  }
  | USE SymbolOrType {
    char *p = strrchr($2, '.');
    opi_assert(p);
    $$ = opi_ast_use($2, p + 1);
    free($2);
  }
  | USE Type '.' '*' {
    size_t len = strlen($2) + 1;
    char buf[len + 1];
    sprintf(buf, "%s.", $2);
    $$ = opi_ast_use(buf, "*");
    free($2);
  }
;

fields
  : fields_aux
  | fields_aux ','
;

fields_aux
  : {
    $$ = malloc(sizeof(struct cod_strvec));
    cod_strvec_init($$);
  }
  | SYMBOL {
    $$ = malloc(sizeof(struct cod_strvec));
    cod_strvec_init($$);
    cod_strvec_push($$, $1);
    free($1);
  }
  | fields_aux ',' SYMBOL {
    $$ = $1;
    cod_strvec_push($$, $3);
    free($3);
  }
;

anyfn_aux: fn_aux | vafn_aux;

fn_aux
  : param RARROW Expr {
    $$ = opi_ast_fn_new_with_patterns($1.data, $1.len, $3);
    cod_vec_destroy($1);
  }
;

vafn_aux
  : DOTDOT SYMBOL RARROW Expr {
    char *p[] = { $2 };
    OpiAst *fn = opi_ast_fn(p, 1, $4);
    OpiAst *param[] = { opi_ast_const(opi_num_new(0)), fn };
    $$ = opi_ast_apply(opi_ast_var("vaarg"), param, 2);
    $$->apply.loc = location(&@$);
    free($2);
  }
  | param DOTDOT SYMBOL RARROW Expr {
    cod_vec_push($1, opi_ast_pattern_new_ident($3));
    OpiAst *fn = opi_ast_fn_new_with_patterns($1.data, $1.len, $5);
    OpiAst *param[] = { opi_ast_const(opi_num_new($1.len - 1)), fn };
    $$ = opi_ast_apply(opi_ast_var("vaarg"), param, 2);
    $$->apply.loc = location(&@$);
    cod_vec_destroy($1);
    free($3);
  }
;

anydef_aux: def_aux | vadef_aux;

def_aux
  : param '=' Expr {
    $$ = opi_ast_fn_new_with_patterns($1.data, $1.len, $3);
    cod_vec_destroy($1);
  }
;

vadef_aux
  : DOTDOT SYMBOL '=' Expr {
    char *p[] = { $2 };
    OpiAst *fn = opi_ast_fn(p, 1, $4);
    OpiAst *param[] = { opi_ast_const(opi_num_new(0)), fn };
    $$ = opi_ast_apply(opi_ast_var("vaarg"), param, 2);
    $$->apply.loc = location(&@$);
    free($2);
  }
  | param DOTDOT SYMBOL '=' Expr {
    cod_vec_push($1, opi_ast_pattern_new_ident($3));
    OpiAst *fn = opi_ast_fn_new_with_patterns($1.data, $1.len, $5);
    OpiAst *param[] = { opi_ast_const(opi_num_new($1.len - 1)), fn };
    $$ = opi_ast_apply(opi_ast_var("vaarg"), param, 2);
    $$->apply.loc = location(&@$);
    cod_vec_destroy($1);
    free($3);
  }
;

binds
  : SYMBOL '=' Expr {
    $$ = binds_new();
    binds_push($$, $1, $3);
    free($1);
  }
  | SYMBOL anydef_aux {
    $$ = binds_new();
    binds_push($$, $1, $2);
    free($1);
  }
  | binds AND SYMBOL '=' Expr {
    $$ = $1;
    binds_push($$, $3, $5);
    free($3);
  }
  | binds AND SYMBOL anydef_aux {
    $$ = $1;
    binds_push($$, $3, $4);
    free($3);
  }
;

recbinds
  : SYMBOL '=' lambda {
    $$ = binds_new();
    binds_push($$, $1, $3);
    free($1);
  }
  | SYMBOL def_aux {
    $$ = binds_new();
    binds_push($$, $1, $2);
    free($1);
  }
  | recbinds AND SYMBOL '=' lambda {
    $$ = $1;
    binds_push($$, $3, $5);
    free($3);
  }
  | recbinds AND SYMBOL def_aux {
    $$ = $1;
    binds_push($$, $3, $4);
    free($3);
  }
;

string: str_aux {
  if ($1.p.len == 0) {
    $$ = opi_ast_const(opi_str_drain_with_len($1.s.data, $1.s.len - 1));
    cod_vec_destroy($1.p);
  } else {
    char *fmt = $1.s.data;
    OpiAst *p[$1.p.len + 1];
    p[0] = opi_ast_const(opi_str_drain_with_len($1.s.data, $1.s.len - 1));
    for (size_t i = 0; i < $1.p.len; ++i)
      p[i + 1] = $1.p.data[i];
    cod_vec_destroy($1.p);
    $$ = opi_ast_apply(opi_ast_var("format"), p, $1.p.len + 1);
  }
};
str_aux
  : '"' fmt '"' { $$ = $2; cod_vec_push($$.s, 0); }
  | 'q' fmt 'q' { $$ = $2; cod_vec_push($$.s, 0); }
;

shell: shell_aux {
  OpiAst *cmd;
  if ($1.p.len == 0) {
    cmd = opi_ast_const(opi_str_drain_with_len($1.s.data, $1.s.len - 1));
    cod_vec_destroy($1.p);
  } else {
    char *fmt = $1.s.data;
    OpiAst *p[$1.p.len + 1];
    p[0] = opi_ast_const(opi_str_drain_with_len($1.s.data, $1.s.len - 1));
    for (size_t i = 0; i < $1.p.len; ++i)
      p[i + 1] = $1.p.data[i];
    cod_vec_destroy($1.p);
    cmd = opi_ast_apply(opi_ast_var("format"), p, $1.p.len + 1);
  }
  $$ = opi_ast_apply(opi_ast_var("shell"), &cmd, 1);
  $$->apply.loc = location(&@$);
};
shell_aux: '`' fmt '`' { $$ = $2; cod_vec_push($$.s, 0); };

qr: qr_aux {
  OpiAst *str;
  if ($1.p.len == 0) {
    str = opi_ast_const(opi_str_drain_with_len($1.s.data, $1.s.len - 1));
    cod_vec_destroy($1.p);
  } else {
    char *fmt = $1.s.data;
    OpiAst *p[$1.p.len + 1];
    p[0] = opi_ast_const(opi_str_drain_with_len($1.s.data, $1.s.len - 1));
    for (size_t i = 0; i < $1.p.len; ++i)
      p[i + 1] = $1.p.data[i];
    cod_vec_destroy($1.p);
    str = opi_ast_apply(opi_ast_var("format"), p, $1.p.len + 1);
  }
  $$ = opi_ast_apply(opi_ast_var("regex"), &str, 1);
  $$->apply.loc = location(&@$);
};
qr_aux: 'r' fmt 'q' { $$ = $2; cod_vec_push($$.s, 0); };

sr: S qr string {
  OpiAst *p[] = { $2, $3, opi_ast_const(opi_str_drain($1)) };
  $$ = opi_ast_apply(opi_ast_var("__builtin_sr"), p, 3);
};

fmt
  : {
    cod_vec_init($$.s);
    cod_vec_init($$.p);
  }
  | fmt CHAR {
    $$ = $1;
    cod_vec_push($$.s, $2);
  }
  | fmt FMT_START Expr FMT_END {
    $$ = $1;
    cod_vec_push($$.s, '%');
    cod_vec_push($$.s, 'd');
    cod_vec_push($$.p, $3);
  }
;

table: '{' table_binds '}' { $$ = $2; }
table_binds
  : table_aux {
    OpiAst *list_args[$1.names.size];
    for (size_t i = 0; i < $1.names.size; ++i) {
      OpiAst *key = opi_ast_const(opi_symbol($1.names.data[i]));
      OpiAst *val = opi_ast_var($1.names.data[i]);
      list_args[i] = opi_ast_binop(OPI_OPC_CONS, key, val);
    }

    OpiAst *list = opi_ast_apply(opi_ast_var("List"), list_args, $1.names.size);
    cod_strvec_destroy(&$1.names);

    cod_vec_push($1.body, list);
    OpiAst *block = opi_ast_block($1.body.data, $1.body.len);
    cod_vec_destroy($1.body);

    $$ = opi_ast_apply(opi_ast_var("Table"), &block, 1);
  }
;

table_aux
  : {
    cod_strvec_init(&$$.names);
    cod_vec_init($$.body);
  }
  | table_aux LET REC recbinds {
    OpiAst *letrec = opi_ast_fix($4->vars.data, (OpiAst**)$4->vals.data,
        $4->vars.size, NULL);

    $$ = $1;
    for (size_t i = 0; i < $4->vars.size; ++i)
      cod_strvec_push(&$$.names, $4->vars.data[i]);
    cod_vec_push($$.body, letrec);

    binds_delete($4);
  }
  | table_aux LET binds {
    OpiAst *let = opi_ast_let($3->vars.data, (OpiAst**)$3->vals.data,
        $3->vars.size, NULL);

    $$ = $1;
    for (size_t i = 0; i < $3->vars.size; ++i)
      cod_strvec_push(&$$.names, $3->vars.data[i]);
    cod_vec_push($$.body, let);

    binds_delete($3);
  }
;

cond_table_aux
  : {
    cod_strvec_init(&$$.names);
    cod_vec_init($$.body);
  }
  | cond_table_aux LET REC recbinds {
    OpiAst *letrec = opi_ast_fix($4->vars.data, (OpiAst**)$4->vals.data,
        $4->vars.size, NULL);

    $$ = $1;
    for (size_t i = 0; i < $4->vars.size; ++i)
      cod_strvec_push(&$$.names, $4->vars.data[i]);
    cod_vec_push($$.body, letrec);

    binds_delete($4);
  }
  | cond_table_aux LET binds {
    OpiAst *let = opi_ast_let($3->vars.data, (OpiAst**)$3->vals.data,
        $3->vars.size, NULL);

    $$ = $1;
    for (size_t i = 0; i < $3->vars.size; ++i)
      cod_strvec_push(&$$.names, $3->vars.data[i]);
    cod_vec_push($$.body, let);

    binds_delete($3);
  }
  | cond_table_aux LET SYMBOL {
    $$ = $1;
    cod_strvec_push(&$$.names, $3);
    cod_vec_push($$.body, NULL);
    free($3);
  }
;
%%

start_token_t opi_start_token;

static const char*
filename(FILE *fp)
{
  char proclnk[0xFFF];
  static char filename[PATH_MAX];
  int fno;
  ssize_t r;

  fno = fileno(fp);
  sprintf(proclnk, "/proc/self/fd/%d", fno);
  r = readlink(proclnk, filename, PATH_MAX);
  if (r < 0)
    return NULL;
  filename[r] = '\0';
  return filename;
}

OpiAst*
opi_parse(FILE *in)
{
  g_errorptr = NULL;
  cod_vec_push(opi_start_token, START_FILE);
  const char *path = filename(in);
  if (path)
    strcpy(g_filename, path);
  else
    g_filename[0] = 0;

  OpiScanner *scanner = opi_scanner();
  opi_scanner_set_in(scanner, in);
  yyparse(scanner);
  opi_scanner_delete(scanner);
  return g_result;
}

OpiAst*
opi_parse_expr(OpiScanner *scanner, char **errorptr)
{
  g_errorptr = errorptr;
  cod_vec_push(opi_start_token, START_REPL);
  g_filename[0] = 0;

  yyparse(scanner);
  return g_result;
}

void
yyerror(void *locp_ptr, OpiScanner *yyscanner, const char *what)
{
  YYLTYPE *locp = locp_ptr;

  opi_error = 1;

  if (g_errorptr == NULL) {
    opi_error("parse error: %s\n", what);
  } else {
    *g_errorptr = strdup(what);
    return;
  }

  const char *path = filename(opi_scanner_get_in(yyscanner));
  if (path) {
    opi_error("%s:%d:%d\n", path, locp->first_line, locp->first_column);
    opi_assert(opi_show_location(OPI_ERROR, path, locp->first_column,
        locp->first_line, locp->last_column, locp->last_line) == OPI_OK);
  }
}

static OpiLocation*
location(void *locp_ptr)
{
  YYLTYPE *locp = locp_ptr;
  if (g_filename[0]) {
    return opi_location_new(g_filename, locp->first_line, locp->first_column,
                        locp->last_line, locp->last_column);
  } else {
    return NULL;
  }
}

