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

"::" { return DCOL; }

\"(\\.|[^"\\])*\" {
  size_t len = strlen(yytext) - 2;
  char buf[len + 1];
  memcpy(buf, yytext + 1, len);
  buf[len] = 0;
  
  char *str = malloc(len + 1);
  char *p = str;
  for (size_t i = 0; i < len + 1; ++i) {
    switch (buf[i]) {
      case '\\':
        switch (buf[++i]) {
          case 'a': *p++ = '\a'; break;
          case 'b': *p++ = '\b'; break;
          case 'e': *p++ = '\x1b'; break;
          case 'f': *p++ = '\f'; break;
          case 'n': *p++ = '\n'; break;
          case 'r': *p++ = '\r'; break;
          case 't': *p++ = '\t'; break;
          case 'v': *p++ = '\v'; break;
          case '?': *p++ = '\?'; break;
          default: *p++ = buf[i];
        }
        break;

      default:
        *p++ = buf[i];
    }
  }
  yylval.str = str;
  return STRING;
}
`(\\.|[^`\\])*` {
  size_t len = strlen(yytext) - 2;
  char buf[len + 1];
  memcpy(buf, yytext + 1, len);
  buf[len] = 0;
  
  char *str = malloc(len + 1);
  char *p = str;
  for (size_t i = 0; i < len + 1; ++i) {
    switch (buf[i]) {
      case '\\':
        switch (buf[++i]) {
          case 'a': *p++ = '\a'; break;
          case 'b': *p++ = '\b'; break;
          case 'e': *p++ = '\x1b'; break;
          case 'f': *p++ = '\f'; break;
          case 'n': *p++ = '\n'; break;
          case 'r': *p++ = '\r'; break;
          case 't': *p++ = '\t'; break;
          case 'v': *p++ = '\v'; break;
          case '?': *p++ = '\?'; break;
          default: *p++ = buf[i];
        }
        break;

      default:
        *p++ = buf[i];
    }
  }
  yylval.str = str;
  return SHELL;
}
nil { yylval.opi = opi_nil; return CONST; }
true { yylval.opi = opi_true; return CONST; }
false { yylval.opi = opi_false; return CONST; }
stdin { yylval.opi = opi_stdin; return CONST; }
stdout { yylval.opi = opi_stdout; return CONST; }
stderr { yylval.opi = opi_stderr; return CONST; }

not { return NOT; }
is { return IS; }
eq { return EQ; }
equal { return EQUAL; }
is[ \n\t]+not { return ISNOT; }

wtf { return WTF; }

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
"-|" { return COMPOSE; }

";"+ { return ';'; }
"@"|"!" |
"$"|":" { return yytext[0]; }
"->" { return RARROW; }
"="|"+"|"-"|"*"|"/"|"%"|\. { return yytext[0]; }
"["|"]"|"{"|"}"|"\\"|"("|")"|"," { return yytext[0]; }

"'"[^ \t\n(){}\[\]'\";,:]+ { yylval.opi = opi_symbol(yytext+1); return CONST; }
[a-zA-Z_][0-9a-zA-Z_]*['?]? { yylval.str = strdup(yytext); return SYMBOL; }

. { fprintf(stderr, "unexpected symbol: %s\n", yytext); exit(1); }

%%
