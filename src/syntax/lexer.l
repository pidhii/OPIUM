%{
#include <string.h>
#include "opium/opium.h"
#include <ctype.h>

#define YY_DECL \
  int yylex(YYSTYPE *yylval, YYLTYPE *yylloc, OpiScanner *yyscanner)

#define YY_USER_ACTION                          \
    yylloc->first_line = yylloc->last_line;     \
    yylloc->first_column = yylloc->last_column; \
    for(int i = 0; yytext[i] != '\0'; i++) {    \
        if(yytext[i] == '\n') {                 \
            yylloc->last_line++;                \
            yylloc->last_column = 1;            \
        }                                       \
        else {                                  \
            yylloc->last_column++;              \
        }                                       \
    }

#include "parser.h"

extern
cod_vec(int) opi_start_token;

enum {
  BRACE_CODE,
  BRACE_STRING,
  BRACE_SHELL,
  BRACE_QQ1, // [...]
  BRACE_QQ2, // (...)
  BRACE_QQ3, // {...}
  BRACE_QQ4, // /.../
  BRACE_QQ5, // |...|
  BRACE_QQ6, // +...+
  BRACE_SR1, // /../../
  BRACE_SR2, // |..|..|
  BRACE_SR3, // +..+..+
};

static
cod_vec(int) g_braces;
%}

%option noyywrap
%option reentrant
%option interactive

%x comment
%x string
%x shell
%x qq1 qq2 qq3 qq4 qq5 qq6
%x sr1 sr2 sr3

IDENT [a-z_][0-9a-zA-Z_]*['?]?
TYPE_IDENT [A-Z][0-9a-zA-Z_]*
OP_CHAR "="|"+"|"-"|"*"|"/"|"%"|\.|">"|"<"|":"|"\\"|"|"|"&"|"$"|"!"|"@"

%%

%{
  if (opi_start_token.len > 0) {
    int tok = cod_vec_pop(opi_start_token);
    if (tok == START_REPL)
      yy_set_interactive(TRUE);
    if (tok == DUMMYVAR) {
      yylval->str = strdup("$dummyvar");
      return SYMBOL;
    }
    return tok;
  }
%}

[ \t\n]

"#".*$


nan |
[-+]?inf |
[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)? { yylval->num = strtold(yytext, NULL); return NUMBER; }

let { return LET; }
mut { return MUT; }
rec { return REC; }
if { return IF; }
unless { return UNLESS; }
when { return WHEN; }
then { return THEN; }
else { return ELSE; }
in { return IN; }
and { return AND; }
or { return OR; }
load { return LOAD; }
module { return MODULE; }
struct { return STRUCT; }
use { return USE; }
as { return AS; }
return { return RETURN; }
fn { return FN; }
lazy { return LAZY; }
assert { return ASSERT; }
trait { return TRAIT; }
impl { return IMPL; }
for { return FOR; }
in[ \n\t]*"|>" {
  cod_vec_push(opi_start_token, RPIPE);
  cod_vec_push(opi_start_token, DUMMYVAR);
  cod_vec_push(opi_start_token, RARROW);
  cod_vec_push(opi_start_token, DUMMYVAR);
  cod_vec_push(opi_start_token, FN);
  return IN;
}

begin { return BEG; }
end { return END; }

\" { BEGIN(string); return '"'; }
\` { BEGIN(shell); return '`'; }

qq"[" {  BEGIN(qq1); return 'q'; }
qq"(" {  BEGIN(qq2); return 'q'; }
qq"{" {  BEGIN(qq3); return 'q'; }
qq"/" {  BEGIN(qq4); return 'q'; }
qq"|" {  BEGIN(qq5); return 'q'; }
qq"+" {  BEGIN(qq6); return 'q'; }

qr"[" {  BEGIN(qq1); return 'r'; }
qr"(" {  BEGIN(qq2); return 'r'; }
qr"{" {  BEGIN(qq3); return 'r'; }
qr"/" {  BEGIN(qq4); return 'r'; }
qr"|" {  BEGIN(qq5); return 'r'; }
qr"+" {  BEGIN(qq6); return 'r'; }

s[g]*[/|+] {
  switch (yytext[yyleng - 1]) {
    case '/': BEGIN(sr1); break;
    case '|': BEGIN(sr2); break;
    case '+': BEGIN(sr3); break;
  }
  char *start = yytext + 1;
  while (TRUE) {
    if (*start == 'g') start++;
    else break;
  }
  int len = (start - yytext) - 1;
  char buf[len + 1];
  memcpy(buf, yytext + 1, len);
  buf[len] = 0;
  yylval->str = strdup(buf);
  cod_vec_push(opi_start_token, 'r');
  return S;
}

<string>\" { BEGIN(INITIAL); return '"'; }
<shell>\` { BEGIN(INITIAL); return '`'; }

<qq1>"]" { BEGIN(INITIAL); return 'q'; }
<qq2>")" { BEGIN(INITIAL); return 'q'; }
<qq3>"}" { BEGIN(INITIAL); return 'q'; }
<qq4>"/" { BEGIN(INITIAL); return 'q'; }
<qq5>"|" { BEGIN(INITIAL); return 'q'; }
<qq6>"+" { BEGIN(INITIAL); return 'q'; }

<qq1>"]"[a-zA-Z]+ { BEGIN(INITIAL); yylval->str = strdup(yytext+1); cod_vec_push(opi_start_token, 'q'); return R_OPTS; }
<qq2>")"[a-zA-Z]+ { BEGIN(INITIAL); yylval->str = strdup(yytext+1); cod_vec_push(opi_start_token, 'q'); return R_OPTS; }
<qq3>"}"[a-zA-Z]+ { BEGIN(INITIAL); yylval->str = strdup(yytext+1); cod_vec_push(opi_start_token, 'q'); return R_OPTS; }
<qq4>"/"[a-zA-Z]+ { BEGIN(INITIAL); yylval->str = strdup(yytext+1); cod_vec_push(opi_start_token, 'q'); return R_OPTS; }
<qq5>"|"[a-zA-Z]+ { BEGIN(INITIAL); yylval->str = strdup(yytext+1); cod_vec_push(opi_start_token, 'q'); return R_OPTS; }
<qq6>"+"[a-zA-Z]+ { BEGIN(INITIAL); yylval->str = strdup(yytext+1); cod_vec_push(opi_start_token, 'q'); return R_OPTS; }

<sr1>"/" {  BEGIN(qq4); cod_vec_push(opi_start_token, 'q'); return 'q'; }
<sr2>"|" {  BEGIN(qq5); cod_vec_push(opi_start_token, 'q'); return 'q'; }
<sr3>"+" {  BEGIN(qq6); cod_vec_push(opi_start_token, 'q'); return 'q'; }

<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>\\\n
<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>"\\0" { yylval->c = '\0'; return CHAR; }
<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>"\\a" { yylval->c = '\a'; return CHAR; }
<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>"\\b" { yylval->c = '\b'; return CHAR; }
<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>"\\e" { yylval->c = '\x1b'; return CHAR; }
<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>"\\f" { yylval->c = '\f'; return CHAR; }
<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>"\\n" { yylval->c = '\n'; return CHAR; }
<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>"\\r" { yylval->c = '\r'; return CHAR; }
<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>"\\t" { yylval->c = '\t'; return CHAR; }
<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>"\\v" { yylval->c = '\v'; return CHAR; }
<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>"\\?" { yylval->c = '\?'; return CHAR; }
<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>"\\%" { yylval->c = '%'; return CHAR; }

<string>"\\\"" { yylval->c = '"'; return CHAR; }
<shell>"\\`" { yylval->c = '`'; return CHAR; }
<qq1>"\\]" { yylval->c = ']'; return CHAR; }
<qq2>"\\)" { yylval->c = ')'; return CHAR; }
<qq3>"\\}" { yylval->c = '}'; return CHAR; }
<qq4,sr1>"\\/" { yylval->c = '/'; return CHAR; }
<qq5,sr2>"\\|" { yylval->c = '|'; return CHAR; }
<qq6,sr3>"\\+" { yylval->c = '+'; return CHAR; }

<string>"%{" { BEGIN(INITIAL); cod_vec_push(g_braces, BRACE_STRING); return FMT_START; }
<shell>"%{" { BEGIN(INITIAL); cod_vec_push(g_braces, BRACE_SHELL); return FMT_START; }

<qq1>"%{" { BEGIN(INITIAL); cod_vec_push(g_braces, BRACE_QQ1); return FMT_START; }
<qq2>"%{" { BEGIN(INITIAL); cod_vec_push(g_braces, BRACE_QQ2); return FMT_START; }
<qq3>"%{" { BEGIN(INITIAL); cod_vec_push(g_braces, BRACE_QQ3); return FMT_START; }
<qq4>"%{" { BEGIN(INITIAL); cod_vec_push(g_braces, BRACE_QQ4); return FMT_START; }
<qq5>"%{" { BEGIN(INITIAL); cod_vec_push(g_braces, BRACE_QQ5); return FMT_START; }
<qq6>"%{" { BEGIN(INITIAL); cod_vec_push(g_braces, BRACE_QQ6); return FMT_START; }

<sr1>"%{" { BEGIN(INITIAL); cod_vec_push(g_braces, BRACE_SR1); return FMT_START; }
<sr2>"%{" { BEGIN(INITIAL); cod_vec_push(g_braces, BRACE_SR1); return FMT_START; }
<sr3>"%{" { BEGIN(INITIAL); cod_vec_push(g_braces, BRACE_SR1); return FMT_START; }

<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>.  { yylval->c = yytext[0]; return CHAR; }
<string,shell,qq1,qq2,qq3,qq4,qq5,qq6,sr1,sr2,sr3>\n { yylval->c = yytext[0]; return CHAR; }

nil { yylval->opi = opi_nil; return CONST; }
true { yylval->opi = opi_true; return CONST; }
false { yylval->opi = opi_false; return CONST; }
stdin { yylval->opi = opi_stdin; return CONST; }
stdout { yylval->opi = opi_stdout; return CONST; }
stderr { yylval->opi = opi_stderr; return CONST; }

not { return NOT; }
is { return IS; }
eq { return EQ; }
equal { return EQUAL; }
is[ \n\t]+not { return ISNOT; }

"{" {
  cod_vec_push(g_braces, BRACE_CODE);
  return '{';
}
"}" {
  opi_assert(g_braces.len > 0);

  int brace = g_braces.data[g_braces.len - 1];
  cod_vec_pop(g_braces);

  switch (brace & 0xFF) {
    case BRACE_CODE:
      return '}';

    case BRACE_STRING:
      BEGIN(string);
      return FMT_END;

    case BRACE_SHELL:
      BEGIN(shell);
      return FMT_END;

    case BRACE_QQ1:
      BEGIN(qq1);
      return FMT_END;

    case BRACE_QQ2:
      BEGIN(qq2);
      return FMT_END;

    case BRACE_QQ3:
      BEGIN(qq3);
      return FMT_END;

    case BRACE_QQ4:
      BEGIN(qq4);
      return FMT_END;

    case BRACE_QQ5:
      BEGIN(qq5);
      return FMT_END;

    case BRACE_QQ6:
      BEGIN(qq6);
      return FMT_END;

    case BRACE_SR1:
      BEGIN(sr1);
      return FMT_END;

    case BRACE_SR2:
      BEGIN(sr2);
      return FMT_END;

    case BRACE_SR3:
      BEGIN(sr3);
      return FMT_END;

    default:
      opi_assert(!"WTF");
  }
}


"&&" { return SCAND; }
"||" { return SCOR; }
"<" { return NUMLT; }
">" { return NUMGT; }
"<=" { return NUMLE; }
">=" { return NUMGE; }
"==" { return NUMEQ; }
"/=" { return NUMNE; }
"++" { return PLUSPLUS; }
".." { return DOTDOT; }
"." { return '.'; }
"::" { return TABLEREF; }
";" { return ';'; }
";;" { return BRK; }
"@"|"!" |
"$"|":" { return yytext[0]; }
"->" { return RARROW; }
"<-" { return LARROW; }
"="|"+"|"-"|"*"|"/"|"%"|"&"|\.|"^" |
"["|"]"|"("|")"|","|"|" { return yytext[0]; }
"\\" { return FN; }
"|>" { return RPIPE; }

"'"[^ \t\n(){}\[\]'\";,:]+ { yylval->opi = opi_symbol(yytext+1); return CONST; }
{IDENT} { yylval->str = strdup(yytext); return SYMBOL; }
{TYPE_IDENT} { yylval->str = strdup(yytext); return TYPE; }

"("({OP_CHAR}+|is|eq|equal|not|(is[ \n\t]+not)|(not[ \n\t]+(eq|equal))|mod|fmod)")" {
  char *start = yytext + 1;
  int len = yyleng - 2;
  char buf[len + 1];
  memcpy(buf, start, len);
  buf[len] = 0;
  yylval->str = strdup(buf);
  return SYMBOL;
}

. {
  opi_error("parse error: unexpected symbols, %s\n", yytext);
  opi_error = 1;
  return FLEX_ERROR;
}

%%

void
opi_lexer_init(void)
{
  cod_vec_init(g_braces);
  cod_vec_init(opi_start_token);
}

void
opi_lexer_cleanup(void)
{
  cod_vec_destroy(g_braces);
  cod_vec_destroy(opi_start_token);
}
