/*%glr-parser*/
%define parse.error verbose

%{
#include "opium/opium.h"
#include <stdio.h>
#include <string.h>

extern int
yylex();

extern int
yylex_destroy(void);

extern
FILE* yyin;

void
yyerror(const char *what);

static
struct opi_ast *g_result;

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
binds_push(struct binds *binds, const char *var, struct opi_ast *val)
{
  cod_strvec_push(&binds->vars, var);
  cod_ptrvec_push(&binds->vals, val, NULL);
}
%}

%union {
  opi_t opi;
  struct opi_ast *ast;
  long double num;
  char *str;
  struct binds *binds;
  struct cod_strvec *strvec;
  struct cod_ptrvec *ptrvec;

  struct {
    struct cod_strvec *fields;
    struct cod_strvec *vars;
  } matches;
}

//
// Tokens
//
%right '=' RARROW '\\' '@'

%token<num> NUMBER
%token<str> SYMBOL STRING SHELL
%token<opi> CONST
%left LET REC
%left DEF
%left IF UNLESS THEN ELSE
%right IN
%token AND
%token LOAD
%token DCOL
%token NAMESPACE
%token STRUCT
%token USE AS
%nonassoc RETURN
%token WTF
%token DOTDOT

//
// Patterns
//
%type<ast> Atom
%type<ast> Form
%type<ast> Expr

%type<binds> binds recbins
%type<strvec> param param_aux
%type<ptrvec> arg arg_aux
%type<ast> block block_expr
%type<ptrvec> block_aux
%type<ast> fn_aux vafn_aux anyfn_aux
%type<ast> def_aux vadef_aux anydef_aux
%type<ast> lambda valambda anylambda
%type<str> refstr
%type<matches> matches matches_aux
%type<strvec> fields

%right OR
%right '$'
%left '!'
%right SCOR
%right SCAND
%nonassoc IS ISNOT EQ EQUAL NUMLT NUMGT NUMLE NUMGE NUMEQ NUMNE
%right ':' PLUSPLUS
%right '+' '-'
%left '*' '/' '%'
%right COMPOSE
%right NOT
%left '.'

%left UMINUS

%start entry
%%

entry: block { g_result = $1; }

Atom
  : NUMBER { $$ = opi_ast_const(opi_number($1)); }
  | refstr { $$ = opi_ast_var($1); free($1); }
  | CONST { $$ = opi_ast_const($1); }
  | STRING { $$ = opi_ast_const(opi_string($1)); free($1); }
  | SHELL {
    struct opi_ast *cmd = opi_ast_const(opi_string($1));
    $$ = opi_ast_apply(opi_ast_var("shell"), &cmd, 1);
    free($1);
  }
  | '(' Expr ')' { $$ = $2; }
  | WTF { $$ = opi_ast_var("wtf"); }
  | '[' arg_aux ']' {
    $$ = opi_ast_apply(opi_ast_var("list"), (struct opi_ast**)$2->data, $2->size);
    cod_ptrvec_destroy($2, NULL);
    free($2);
  }
  | '[' ']' { $$ = opi_ast_const(opi_nil); }
  | Atom '.' SYMBOL {
    struct opi_ast *p[] = { $1, opi_ast_const(opi_symbol($3)) };
    $$ = opi_ast_apply(opi_ast_var("."), p, 2);
    free($3);
  }
;

refstr
  : SYMBOL
  | refstr DCOL SYMBOL {
    size_t len = strlen($1) + 2 + strlen($3);
    $$ = malloc(len + 1);
    sprintf($$, "%s::%s", $1, $3);
    free($1);
    free($3);
  }
;

Form
  : Atom
  | Atom arg {
    $$ = opi_ast_apply($1, (struct opi_ast**)$2->data, $2->size);
    cod_ptrvec_destroy($2, NULL);
    free($2);
  }
  | Expr '!' arg {
    $$ = opi_ast_apply($1, (struct opi_ast**)$3->data, $3->size);
    cod_ptrvec_destroy($3, NULL);
    free($3);
  }
;

Expr
  : Form
  | '(' ')' {
    $$ = opi_ast_apply(opi_ast_var("()"), NULL, 0);
  }
  | anylambda
  | '@' Expr {
    struct opi_ast *fn = opi_ast_fn(NULL, 0, $2);
    $$ = opi_ast_apply(opi_ast_var("lazy"), &fn, 1);
  }
  | Expr IS Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("is"), p, 2);
  }
  | Expr EQ Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("eq"), p, 2);
  }
  | Expr EQUAL Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("equal"), p, 2);
  }
  | Expr ISNOT Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("is"), p, 2);
    $$ = opi_ast_apply(opi_ast_var("not"), &$$, 1);
  }
  | Expr NOT EQ Expr {
    struct opi_ast *p[] = { $1, $4 };
    $$ = opi_ast_apply(opi_ast_var("eq"), p, 2);
    $$ = opi_ast_apply(opi_ast_var("not"), &$$, 1);
  }
  | Expr NOT EQUAL Expr {
    struct opi_ast *p[] = { $1, $4 };
    $$ = opi_ast_apply(opi_ast_var("equal"), p, 2);
    $$ = opi_ast_apply(opi_ast_var("not"), &$$, 1);
  }
  | NOT Expr {
    struct opi_ast *x = $2;
    $$ = opi_ast_apply(opi_ast_var("not"), &x, 1);
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
    if ($2->tag == OPI_AST_CONST && $2->cnst->type == opi_number_type) {
      long double x = opi_number_get_value($2->cnst);
      opi_as($2->cnst, struct opi_number).val = -x;
      $$ = $2;
    } else {
      struct opi_ast *p[] = { opi_ast_const(opi_number(0)), $2 };
      $$ = opi_ast_apply(opi_ast_var("-"), p, 2);
    }
  }
  | '+' Expr %prec UMINUS { $$ = $2; }
  | Expr '+' Expr { $$ = opi_ast_binop(OPI_OPC_ADD, $1, $3); }
  | Expr '-' Expr { $$ = opi_ast_binop(OPI_OPC_SUB, $1, $3); }
  | Expr '*' Expr { $$ = opi_ast_binop(OPI_OPC_MUL, $1, $3); }
  | Expr '/' Expr { $$ = opi_ast_binop(OPI_OPC_DIV, $1, $3); }
  | Expr '%' Expr { $$ = opi_ast_binop(OPI_OPC_MOD, $1, $3); }
  | Expr ':' Expr { $$ = opi_ast_binop(OPI_OPC_CONS, $1, $3); }
  | Expr COMPOSE Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("-|"), p, 2);
  }
  | Expr PLUSPLUS Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("++"), p, 2);
  }
  | '{' block '}' { $$ = $2; }
  | IF Expr THEN Expr ELSE Expr {
    $$ = opi_ast_if($2, $4, $6);
  }
  | UNLESS Expr THEN Expr ELSE Expr {
    $$ = opi_ast_if($2, $6, $4);
  }
  | LET REC recbins IN Expr {
    $$ = opi_ast_fix($3->vars.data, (struct opi_ast**)$3->vals.data, $3->vars.size, $5);
    binds_delete($3);
  }
  | LET binds IN Expr {
    $$ = opi_ast_let($2->vars.data, (struct opi_ast**)$2->vals.data, $2->vars.size, $4);
    binds_delete($2);
  }
  | LET refstr '{' matches '}' '=' Expr IN Expr {
    struct opi_ast *body[] = {
      opi_ast_match($2, $4.vars->data, $4.fields->data, $4.vars->size, $7, NULL, NULL),
      $9
    };
    $$ = opi_ast_block(body, 2);
    free($2);
    cod_strvec_destroy($4.vars);
    cod_strvec_destroy($4.fields);
    free($4.vars);
    free($4.fields);
  }
  | LET SYMBOL ':' SYMBOL '=' Expr IN Expr {
    char *flds[] = { "car", "cdr" };
    char *vars[] = { $2, $4 };
    struct opi_ast *body[] = {
      opi_ast_match("pair", vars, flds, 2, $6, NULL, NULL),
      $8
    };
    $$ = opi_ast_block(body, 2);
    free($2);
    free($4);
  }
  | IF LET refstr '{' matches '}' '=' Expr THEN Expr ELSE Expr {
    $$ = opi_ast_match($3, $5.vars->data, $5.fields->data, $5.vars->size, $8, $10, $12),
    free($3);
    cod_strvec_destroy($5.vars);
    cod_strvec_destroy($5.fields);
    free($5.vars);
    free($5.fields);
  }
  | IF LET SYMBOL ':' SYMBOL '=' Expr THEN Expr ELSE Expr {
    char *flds[] = { "car", "cdr" };
    char *vars[] = { $3, $5 };
    $$ = opi_ast_match("pair", vars, flds, 2, $7, $9, $11),
    free($3);
    free($5);
  }
  | UNLESS LET refstr '{' matches '}' '=' Expr THEN Expr ELSE Expr {
    $$ = opi_ast_match($3, $5.vars->data, $5.fields->data, $5.vars->size, $8, $12, $10),
    free($3);
    cod_strvec_destroy($5.vars);
    cod_strvec_destroy($5.fields);
    free($5.vars);
    free($5.fields);
  }
  | UNLESS LET SYMBOL ':' SYMBOL '=' Expr THEN Expr ELSE Expr {
    char *flds[] = { "car", "cdr" };
    char *vars[] = { $3, $5 };
    $$ = opi_ast_match("pair", vars, flds, 2, $7, $11, $9),
    free($3);
    free($5);
  }
  | RETURN Expr {
    $$ = opi_ast_return($2);
  }
  | Expr OR Expr {
    $$ = opi_ast_eor($1, $3);
  }
;

matches
  : {
    cod_strvec_init($$.fields = malloc(sizeof(struct cod_strvec)));
    cod_strvec_init($$.vars = malloc(sizeof(struct cod_strvec)));
  }
  | matches_aux
;

matches_aux
  : SYMBOL ':' SYMBOL {
    cod_strvec_init($$.fields = malloc(sizeof(struct cod_strvec)));
    cod_strvec_init($$.vars = malloc(sizeof(struct cod_strvec)));
    cod_strvec_push($$.fields, $1);
    cod_strvec_push($$.vars, $3);
    free($1);
    free($3);
  }
  | SYMBOL {
    cod_strvec_init($$.fields = malloc(sizeof(struct cod_strvec)));
    cod_strvec_init($$.vars = malloc(sizeof(struct cod_strvec)));
    cod_strvec_push($$.fields, $1);
    cod_strvec_push($$.vars, $1);
    free($1);
  }
  | matches_aux ',' SYMBOL ':' SYMBOL {
    $$ = $1;
    cod_strvec_push($$.fields, $3);
    cod_strvec_push($$.vars, $5);
    free($3);
    free($5);
  }
  | matches_aux ',' SYMBOL {
    $$ = $1;
    cod_strvec_push($$.fields, $3);
    cod_strvec_push($$.vars, $3);
    free($3);
  }
;

anylambda: lambda | valambda;

lambda
  : '\\' fn_aux { $$ = $2; }
;

valambda
  : '\\' vafn_aux { $$ = $2; }
;

param
  : '(' ')' {
    $$ = malloc(sizeof(struct cod_strvec));
    cod_strvec_init($$);
  }
  | param_aux
;
param_aux
  : SYMBOL {
    $$ = malloc(sizeof(struct cod_strvec));
    cod_strvec_init($$);
    cod_strvec_push($$, $1);
    free($1);
  }
  | param_aux SYMBOL {
    $$ = $1;
    cod_strvec_push($$, $2);
    free($2);
  }
;

arg
  : '(' ')' {
    $$ = malloc(sizeof(struct cod_ptrvec));
    cod_ptrvec_init($$);
  }
  | arg_aux
  | '$' Expr {
    $$ = malloc(sizeof(struct cod_ptrvec));
    cod_ptrvec_init($$);
    cod_ptrvec_push($$, $2, NULL);
  }
  | arg_aux '$' Expr { $$ = $1; cod_ptrvec_push($$, $3, NULL); }
  | '\\' anyfn_aux {
    $$ = malloc(sizeof(struct cod_ptrvec));
    cod_ptrvec_init($$);
    cod_ptrvec_push($$, $2, NULL);
  }
  | arg_aux '\\' anyfn_aux { $$ = $1; cod_ptrvec_push($$, $3, NULL); }
;

arg_aux
  : Atom {
    $$ = malloc(sizeof(struct cod_ptrvec));
    cod_ptrvec_init($$);
    cod_ptrvec_push($$, $1, NULL);
  }
  | arg_aux Atom {
    $$ = $1;
    cod_ptrvec_push($$, $2, NULL);
  }
;

block
  : { $$ = opi_ast_block(NULL, 0); }
  | block_aux {
    $$ = opi_ast_block((struct opi_ast**)$1->data, $1->size);
    cod_ptrvec_destroy($1, NULL);
    free($1);
  }
  | block_aux ';' {
    $$ = opi_ast_block((struct opi_ast**)$1->data, $1->size);
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
  | block_aux ';' block_expr {
    $$ = $1;
    cod_ptrvec_push($$, $3, NULL);
  }
;

block_expr
  : Expr
  | IF Expr THEN Expr {
    $$ = opi_ast_if($2, $4, opi_ast_const(opi_nil));
  }
  | UNLESS Expr THEN Expr {
    $$ = opi_ast_if($2, opi_ast_const(opi_nil), $4);
  }
  | LET REC recbins {
    $$ = opi_ast_fix($3->vars.data, (struct opi_ast**)$3->vals.data, $3->vars.size, NULL);
    binds_delete($3);
  }
  | LET binds {
    $$ = opi_ast_let($2->vars.data, (struct opi_ast**)$2->vals.data, $2->vars.size, NULL);
    binds_delete($2);
  }
  | LET refstr '{' matches '}' '=' Expr {
    $$ = opi_ast_match($2, $4.vars->data, $4.fields->data, $4.vars->size, $7, NULL, NULL),
    free($2);
    cod_strvec_destroy($4.vars);
    cod_strvec_destroy($4.fields);
    free($4.vars);
    free($4.fields);
  }
  | LET SYMBOL ':' SYMBOL '=' Expr {
    char *flds[] = { "car", "cdr" };
    char *vars[] = { $2, $4 };
    $$ = opi_ast_match("pair", vars, flds, 2, $6, NULL, NULL),
    free($2);
    free($4);
  }
  | LOAD STRING {
    $$ = opi_ast_load($2);
    free($2);
  }
  | NAMESPACE SYMBOL '{' block '}' {
    $$ = $4;
    char prefix[strlen($2) + 3];
    sprintf(prefix, "%s::", $2);
    opi_ast_block_set_namespace($$, prefix);
    opi_ast_block_set_drop($$, FALSE);
    free($2);
  }
  | STRUCT SYMBOL '{' fields '}' {
    $$ = opi_ast_struct($2, $4->data, $4->size);
    free($2);
    cod_strvec_destroy($4);
    free($4);
  }
  | USE refstr AS SYMBOL {
    $$ = opi_ast_use($2, $4);
    free($2);
    free($4);
  }
  | USE refstr {
    char *p = strrchr($2, ':');
    opi_assert(p);
    $$ = opi_ast_use($2, p + 1);
    free($2);
  }
  | USE refstr DCOL '*' {
    size_t len = strlen($2) + 2;
    char buf[len + 1];
    sprintf(buf, "%s::", $2);
    $$ = opi_ast_use(buf, "*");
    free($2);
  }
;

fields
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
  | fields ',' SYMBOL {
    $$ = $1;
    cod_strvec_push($$, $3);
    free($3);
  }
;

anyfn_aux: fn_aux | vafn_aux;

fn_aux
  : param RARROW Expr {
    $$ = opi_ast_fn($1->data, $1->size, $3);
    cod_strvec_destroy($1);
    free($1);
  }
;

vafn_aux
  : DOTDOT SYMBOL RARROW Expr {
    struct opi_ast *fn = opi_ast_fn(&($2), 1, $4);
    struct opi_ast *param[] = { opi_ast_const(opi_number(0)), fn };
    $$ = opi_ast_apply(opi_ast_var("vaarg"), param, 2);
    free($2);
  }
  | param DOTDOT SYMBOL RARROW Expr {
    cod_strvec_push($1, $3);
    struct opi_ast *fn = opi_ast_fn($1->data, $1->size, $5);
    struct opi_ast *param[] = { opi_ast_const(opi_number($1->size - 1)), fn };
    $$ = opi_ast_apply(opi_ast_var("vaarg"), param, 2);
    cod_strvec_destroy($1);
    free($1);
    free($3);
  }
;

anydef_aux: def_aux | vadef_aux;

def_aux
  : param '=' Expr {
    $$ = opi_ast_fn($1->data, $1->size, $3);
    cod_strvec_destroy($1);
    free($1);
  }
;

vadef_aux
  : DOTDOT SYMBOL '=' Expr {
    struct opi_ast *fn = opi_ast_fn(&($2), 1, $4);
    struct opi_ast *param[] = { opi_ast_const(opi_number(0)), fn };
    $$ = opi_ast_apply(opi_ast_var("vaarg"), param, 2);
    free($2);
  }
  | param DOTDOT SYMBOL '=' Expr {
    cod_strvec_push($1, $3);
    struct opi_ast *fn = opi_ast_fn($1->data, $1->size, $5);
    struct opi_ast *param[] = { opi_ast_const(opi_number($1->size - 1)), fn };
    $$ = opi_ast_apply(opi_ast_var("vaarg"), param, 2);
    cod_strvec_destroy($1);
    free($1);
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

recbins
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
  | recbins AND SYMBOL '=' lambda {
    $$ = $1;
    binds_push($$, $3, $5);
    free($3);
  }
  | recbins AND SYMBOL def_aux {
    $$ = $1;
    binds_push($$, $3, $4);
    free($3);
  }
;

%%

struct opi_ast*
opi_parse(FILE *in)
{
  yyin = in;
  yyparse();
  yylex_destroy();
  return g_result;
}

void
yyerror(const char *what)
{
  fprintf(stderr, "parse error: %s\n", what);
  abort();
}

