%{
#include <string.h>
#include "opium/opium.h"

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
int opi_start_token;

static
int g_begin_cnt = 0;

enum {
  BRACE_CODE,
  BRACE_STRING,
  BRACE_SHELL,
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

%%

%{
  if (opi_start_token >= 0) {
    int tok = opi_start_token;
    opi_start_token = -1;
    if (tok == START_REPL)
      yy_set_interactive(TRUE);
    return tok;
  }
%}

[ \t\n]

^#!.*$
"--".*$
"{-" { BEGIN(comment); }
<comment>\n
<comment>.
<comment>"-}" { BEGIN(INITIAL); }


[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)? { yylval->num = strtold(yytext, NULL); return NUMBER; }

let { return LET; }
rec { return REC; }
if { return IF; }
unless { return UNLESS; }
then { return THEN; }
else { return ELSE; }
in { return IN; }
and { return AND; }
or { return OR; }
load { return LOAD; }
namespace { return NAMESPACE; }
struct { return STRUCT; }
use { return USE; }
as { return AS; }
return { return RETURN; }

begin {
  g_begin_cnt += 1;
  return BEG;
}
end {
  if (--g_begin_cnt)
    opi_start_token = ';';
  return END;
}

"::" { return DCOL; }

\" { BEGIN(string); return '"'; }
<string>\" { BEGIN(INITIAL); return '"'; }
\` { BEGIN(shell); return '`'; }
<shell>\` { BEGIN(INITIAL); return '`'; }
<string,shell>\\\n
<string,shell>\\a { yylval->c = '\a'; return CHAR; }
<string,shell>\\b { yylval->c = '\b'; return CHAR; }
<string,shell>\\e { yylval->c = '\x1b'; return CHAR; }
<string,shell>\\f { yylval->c = '\f'; return CHAR; }
<string,shell>\\n { yylval->c = '\n'; return CHAR; }
<string,shell>\\r { yylval->c = '\r'; return CHAR; }
<string,shell>\\t { yylval->c = '\t'; return CHAR; }
<string,shell>\\v { yylval->c = '\v'; return CHAR; }
<string,shell>\\? { yylval->c = '\?'; return CHAR; }
<string,shell>\\. { yylval->c = yytext[1]; return CHAR; }
<string>"%{" {
  BEGIN(INITIAL);
  cod_vec_push(g_braces, BRACE_STRING);
  return FMT_START;
}
<shell>"%{" {
  BEGIN(INITIAL);
  cod_vec_push(g_braces, BRACE_SHELL);
  return FMT_START;
}
<string,shell>. { yylval->c = yytext[0]; return CHAR; }
<string,shell>\n { yylval->c = yytext[0]; return CHAR; }

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
"." { return COMPOSE; }
"#" { return TABLEREF; }

"{" {
  cod_vec_push(g_braces, BRACE_CODE);
  return '{';
}
"}" {
  opi_assert(g_braces.len > 0);

  int brace = g_braces.data[g_braces.len - 1];
  cod_vec_pop(g_braces);

  switch (brace) {
    case BRACE_CODE:
      return '}';

    case BRACE_STRING:
      BEGIN(string);
      return FMT_END;

    case BRACE_SHELL:
      BEGIN(shell); 
      return FMT_END;

    default:
      opi_assert(!"WTF");
  }
}

";" { return ';'; }
";;" { return BRK; }
"@"|"!" |
"$"|":" { return yytext[0]; }
"->" { return RARROW; }
"="|"+"|"-"|"*"|"/"|"%"|\. { return yytext[0]; }
"["|"]"|"\\"|"("|")"|"," { return yytext[0]; }

"'"[^ \t\n(){}\[\]'\";,:]+ { yylval->opi = opi_symbol(yytext+1); return CONST; }
[a-zA-Z_][0-9a-zA-Z_]*['?]? { yylval->str = strdup(yytext); return SYMBOL; }

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
}

void
opi_lexer_cleanup(void)
{
  cod_vec_destroy(g_braces);
}
