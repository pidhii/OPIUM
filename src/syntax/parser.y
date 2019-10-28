%glr-parser
%define parse.error verbose

%{
#include "opium/opium.h"
#include <stdio.h>

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
  struct opi_strvec vars;
  struct opi_ptrvec vals;
};

static inline struct binds*
binds_new(void)
{
  struct binds *binds = malloc(sizeof(struct binds));
  opi_strvec_init(&binds->vars);
  opi_ptrvec_init(&binds->vals);
  return binds;
}

static inline void
binds_delete(struct binds *binds)
{
  opi_strvec_destroy(&binds->vars);
  opi_ptrvec_destroy(&binds->vals, NULL);
  free(binds);
}

static inline void
binds_push(struct binds *binds, const char *var, struct opi_ast *val)
{
  opi_strvec_push(&binds->vars, var);
  opi_ptrvec_push(&binds->vals, val, NULL);
}
%}

%union {
  opi_t opi;
  struct opi_ast *ast;
  long double num;
  char *str;
  struct binds *binds;
  struct opi_strvec *strvec;
  struct opi_ptrvec *ptrvec;

  struct {
    struct opi_strvec *fields;
    struct opi_strvec *vars;
  } matches;
}

//
// Tokens
//
%right '=' RARROW '\\' '@'

%token<num> NUMBER
%token<str> SYMBOL STRING
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
%token TRAIT IMPL
%nonassoc RETURN
%token WTF


//
// Patterns
//
%type<ast> Atom
%type<ast> Form
%type<ast> Expr

%type<binds> binds fnbinds
%type<strvec> param param_aux
%type<ptrvec> arg arg_aux
%type<ast> block block_expr
%type<ptrvec> block_aux
%type<ast> fn_aux fn_body
%type<ast> def_aux def_body
%type<ast> lambda
%type<str> refstr
%type<matches> matches matches_aux matches_key matches_pos
%type<strvec> fields

%right OR
%right '$' LONGFLUSH
%right SCOR
%right SCAND
%nonassoc IS ISNOT EQ EQUAL NUMLT NUMGT NUMLE NUMGE NUMEQ NUMNE
%right ':' // TODO: ++
%right '+' '-'
%left '*' '/' '%'
// TODO: **
%right '.'
%left AT
%right NOT

%start entry
%%

entry: block { g_result = $1; }

Atom
  : NUMBER { $$ = opi_ast_const(opi_number($1)); }
  | refstr { $$ = opi_ast_var($1); free($1); }
  | CONST { $$ = opi_ast_const($1); }
  | STRING { $$ = opi_ast_const(opi_string($1)); free($1); }
  | '(' Expr ')' { $$ = $2; }
  | WTF { $$ = opi_ast_var("wtf"); }
  | '[' arg_aux ']' {
    $$ = opi_ast_apply(opi_ast_var("list"), (struct opi_ast**)$2->data, $2->size);
    opi_ptrvec_destroy($2, NULL);
    free($2);
  }
  | '!' Atom {
    struct opi_ast *p[] = { $2 };
    $$ = opi_ast_apply(opi_ast_var("!"), p, 1);
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
  | Form arg {
    $$ = opi_ast_apply($1, (struct opi_ast**)$2->data, $2->size);
    opi_ptrvec_destroy($2, NULL);
    free($2);
  }
;

Expr
  : Form
  | '(' ')' {
    $$ = opi_ast_apply(opi_ast_var("()"), NULL, 0);
  }
  | lambda
  | '@' Expr {
    struct opi_ast *fn = opi_ast_fn(NULL, 0, $2);
    $$ = opi_ast_apply(opi_ast_var("lazy"), &fn, 1);
  }
  | LONGFLUSH Expr {
    struct opi_ast *p[] = { $2 };
    $$ = opi_ast_apply(opi_ast_var("!"), p, 1);
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
  | Expr NUMLT Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("<"), p, 2);
  }
  | Expr NUMGT Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var(">"), p, 2);
  }
  | Expr NUMLE Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("<="), p, 2);
  }
  | Expr NUMGE Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var(">="), p, 2);
  }
  | Expr NUMEQ Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("=="), p, 2);
  }
  | Expr NUMNE Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("/="), p, 2);
  }
  | Expr SCAND Expr {
    $$ = opi_ast_and($1, $3);
  }
  | Expr SCOR Expr {
    $$ = opi_ast_or($1, $3);
  }
  | Expr '-' Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("-"), p, 2);
  }
  | Expr '+' Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("+"), p, 2);
  }
  | Expr '*' Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("*"), p, 2);
  }
  | Expr '/' Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("/"), p, 2);
  }
  | Expr '%' Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("%"), p, 2);
  }
  | Expr ':' Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var(":"), p, 2);
  }
  | Expr '.' Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("."), p, 2);
  }
  | Expr AT Expr {
    struct opi_ast *p[] = { $1, $3 };
    $$ = opi_ast_apply(opi_ast_var("!!"), p, 2);
  }
  | Expr '$' Expr {
    struct opi_ast *p[] = { $3 };
    $$ = opi_ast_apply($1, p, 1);
  }
  | '{' block '}' { $$ = $2; }
  | IF Expr THEN Expr ELSE Expr {
    $$ = opi_ast_if($2, $4, $6);
  }
  | UNLESS Expr THEN Expr ELSE Expr {
    $$ = opi_ast_if($2, $6, $4);
  }
  | LET REC fnbinds IN Expr {
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
    opi_strvec_destroy($4.vars);
    opi_strvec_destroy($4.fields);
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
    opi_strvec_destroy($5.vars);
    opi_strvec_destroy($5.fields);
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
    opi_strvec_destroy($5.vars);
    opi_strvec_destroy($5.fields);
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
    opi_strvec_init($$.fields = malloc(sizeof(struct opi_strvec)));
    opi_strvec_init($$.vars = malloc(sizeof(struct opi_strvec)));
  }
  | matches_aux
;

matches_aux: matches_pos | matches_key;

matches_key
  : SYMBOL ':' SYMBOL {
    opi_strvec_init($$.fields = malloc(sizeof(struct opi_strvec)));
    opi_strvec_init($$.vars = malloc(sizeof(struct opi_strvec)));
    opi_strvec_push($$.fields, $1);
    opi_strvec_push($$.vars, $3);
    free($1);
    free($3);
  }
  | matches_aux ',' SYMBOL ':' SYMBOL {
    $$ = $1;
    opi_strvec_push($$.fields, $3);
    opi_strvec_push($$.vars, $5);
    free($3);
    free($5);
  }
;

matches_pos
  : SYMBOL {
    opi_strvec_init($$.fields = malloc(sizeof(struct opi_strvec)));
    opi_strvec_init($$.vars = malloc(sizeof(struct opi_strvec)));
    char buf[42];
    sprintf(buf, "#%zu", $$.fields->size);
    opi_strvec_push($$.fields, buf);
    opi_strvec_push($$.vars, $1);
    free($1);
  }
  | matches_pos ',' SYMBOL {
    $$ = $1;
    char buf[42];
    sprintf(buf, "#%zu", $$.fields->size);
    opi_strvec_push($$.fields, buf);
    opi_strvec_push($$.vars, $3);
    free($3);
  }
;

lambda
  : '\\' fn_aux { $$ = $2; }
  | param RARROW Expr {
    $$ = opi_ast_fn($1->data, $1->size, $3);
    opi_strvec_destroy($1);
    free($1);
  }
;

param
  : '(' ')' {
    $$ = malloc(sizeof(struct opi_strvec));
    opi_strvec_init($$);
  }
  | SYMBOL {
    $$ = malloc(sizeof(struct opi_strvec));
    opi_strvec_init($$);
    opi_strvec_push($$, $1);
    free($1);
  }
  | '(' param_aux ')' {
    $$ = $2;
  }
;
param_aux
  : SYMBOL {
    $$ = malloc(sizeof(struct opi_strvec));
    opi_strvec_init($$);
    opi_strvec_push($$, $1);
    free($1);
  }
  | param_aux ',' SYMBOL {
    $$ = $1;
    opi_strvec_push($$, $3);
    free($3);
  }
;

arg
  : '(' ')' {
    $$ = malloc(sizeof(struct opi_ptrvec));
    opi_ptrvec_init($$);
  }
  | Atom {
    $$ = malloc(sizeof(struct opi_ptrvec));
    opi_ptrvec_init($$);
    opi_ptrvec_push($$, $1, NULL);
  }
  | '(' arg_aux ',' Expr ')' {
    $$ = $2;
    opi_ptrvec_push($$, $4, NULL);
  }
;

arg_aux
  : Expr {
    $$ = malloc(sizeof(struct opi_ptrvec));
    opi_ptrvec_init($$);
    opi_ptrvec_push($$, $1, NULL);
  }
  | arg_aux ',' Expr {
    $$ = $1;
    opi_ptrvec_push($$, $3, NULL);
  }
;

block
  : { $$ = opi_ast_block(NULL, 0); }
  | block_aux {
    $$ = opi_ast_block((struct opi_ast**)$1->data, $1->size);
    opi_ptrvec_destroy($1, NULL);
    free($1);
  }
  | block_aux ';' {
    $$ = opi_ast_block((struct opi_ast**)$1->data, $1->size);
    opi_ptrvec_destroy($1, NULL);
    free($1);
  }
;

block_aux
  : block_expr {
    $$ = malloc(sizeof(struct opi_ptrvec));
    opi_ptrvec_init($$);
    opi_ptrvec_push($$, $1, NULL);
  }
  | block_aux ';' block_expr {
    $$ = $1;
    opi_ptrvec_push($$, $3, NULL);
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
  | LET REC fnbinds {
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
    opi_strvec_destroy($4.vars);
    opi_strvec_destroy($4.fields);
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
    opi_strvec_destroy($4);
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
  | TRAIT SYMBOL def_aux {
    $$ = opi_ast_trait($2, $3);
    free($2);
  }
  | IMPL refstr refstr def_aux {
    $$ = opi_ast_impl($3, $2, $4);
    free($2);
    free($3);
  }
;

fields
  : {
    $$ = malloc(sizeof(struct opi_strvec));
    opi_strvec_init($$);
  }
  | SYMBOL {
    $$ = malloc(sizeof(struct opi_strvec));
    opi_strvec_init($$);
    opi_strvec_push($$, $1);
    free($1);
  }
  | fields ',' SYMBOL {
    $$ = $1;
    opi_strvec_push($$, $3);
    free($3);
  }
;

fn_aux
  : param fn_aux {
    $$ = opi_ast_fn($1->data, $1->size, $2);
    opi_strvec_destroy($1);
    free($1);
  }
  | param fn_body {
    $$ = opi_ast_fn($1->data, $1->size, $2);
    opi_strvec_destroy($1);
    free($1);
  }
;

fn_body
  : RARROW Expr { $$ = $2; }
;

def_aux
  : param def_aux {
    $$ = opi_ast_fn($1->data, $1->size, $2);
    opi_strvec_destroy($1);
    free($1);
  }
  | param def_body {
    $$ = opi_ast_fn($1->data, $1->size, $2);
    opi_strvec_destroy($1);
    free($1);
  }
;

def_body
  : '=' Expr { $$ = $2; }
;

binds
  : SYMBOL '=' Expr {
    $$ = binds_new();
    binds_push($$, $1, $3);
    free($1);
  }
  | SYMBOL def_aux {
    $$ = binds_new();
    binds_push($$, $1, $2);
    free($1);
  }
  | binds AND SYMBOL '=' Expr {
    $$ = $1;
    binds_push($$, $3, $5);
    free($3);
  }
  | binds AND SYMBOL def_aux {
    $$ = $1;
    binds_push($$, $3, $4);
    free($3);
  }
;

fnbinds
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
  | fnbinds AND SYMBOL '=' lambda {
    $$ = $1;
    binds_push($$, $3, $5);
    free($3);
  }
  | fnbinds AND SYMBOL def_aux {
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

