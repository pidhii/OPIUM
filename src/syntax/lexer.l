%option noyywrap

%{
#include <string.h>
#include "opium/opium.h"

#define YY_DECL int yylex()

#include "parser.h"
%}

%%

[ \t\n]

#.*$

[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)? { yylval.num = strtold(yytext, NULL); return NUMBER; }

let { return LET; }
rec { return REC; }
def { return DEF; }
if { return IF; }
then { return THEN; }
else { return ELSE; }
in { return IN; }
and { return AND; }
load { return LOAD; }
namespace { return NAMESPACE; }
struct { return STRUCT; }
use { return USE; }
as { return AS; }
trait { return TRAIT; }
impl { return IMPL; }

"::" { return DCOL; }

\"(\\.|[^"\\])*\" {
  size_t len = strlen(yytext);
  char buf[len - 1];
  memcpy(buf, yytext + 1, len - 2);
  buf[len - 2] = 0;
  yylval.str = strdup(buf);
  return STRING;
}
nil { yylval.opi = opi_nil; return CONST; }
true { yylval.opi = opi_true; return CONST; }
false { yylval.opi = opi_false; return CONST; }
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


";"+ { return ';'; }
"$"|":" { return yytext[0]; }
"->" { return RARROW; }
"="|"+"|"-"|"*"|"/"|"%" { return yytext[0]; }
"{"|"}"|"\\"|"("|")"|"," { return yytext[0]; }

"("("-"|"+"|"*"|"/"|"%")")" {
  size_t len = strlen(yytext);
  char buf[len - 1];
  memcpy(buf, yytext + 1, len - 2);
  buf[len - 2] = 0;
  yylval.str = strdup(buf);
  return SYMBOL;
}
"'"[^ \t\n(){}\[\]'\";,]+ { yylval.opi = opi_symbol(yytext); return CONST; }
[a-zA-Z_][0-9a-zA-Z_]*['?]? { yylval.str = strdup(yytext); return SYMBOL; }

. { fprintf(stderr, "unexpected symbol: %s\n", yytext); exit(1); }

%%
