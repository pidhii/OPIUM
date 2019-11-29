" Vim syntax file
" Language: opium
" Maintainer: Ivan Pidhurskyi

if exists("b:current_syntax")
  finish
endif

set comments=sr:{-,mb:-,ex:-}

set iskeyword+=?,'
syn match opiIdentifier /\<[a-z_][a-zA-Z0-9_]*['?]?\>/
syn match opiType       /\<[A-Z][a-zA-Z0-9_]*\>/
syn match opiSymbol     /'[^ \t\n(){}\[\]'";,:]\+/

syn region opiModuleDef matchgroup=opiModule start=/\<module\>/ end=/\<end\>/ contains=TOP

syn keyword opiType fn

syn region opiTable matchgroup=Type start=/{/ end=/}/ contains=TOP skipwhite skipnl

syn keyword opiUse use as

syn keyword opiStruct struct nextgroup=opiStructName skipwhite skipnl
syn match   opiStructName /\k\+/ contained

syn region opiList matchgroup=opiType start=/\[/ matchgroup=opiType end=/\]/ skipwhite skipnl contains=TOP

"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""'
" Builtins:
syn keyword Function null? undefined? number? symbol? string? boolean? pair? fn? lazy? FILE? table? svector? dvector?
syn keyword Function number table list regex svector dvector
syn keyword Function write display newline print printf fprintf format
syn keyword Function car cdr
syn keyword Function pairs
syn keyword Function apply vaarg
syn keyword Function id
syn keyword Function die error
syn keyword Function force
syn keyword Function system shell
syn keyword Function loadfile
syn keyword Function exit
syn keyword Function next

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
" Base:
syn keyword Function length
syn keyword Function strlen substr strstr chop chomp ltrim trim concat
syn keyword Function open popen
syn keyword Function read readline
" base/common.opi
syn keyword Function flip
" base/list-base.opi
syn keyword Function range
syn keyword Function revappend reverse
syn keyword Function any? all?
syn keyword Function revmap map
syn keyword Function foreach
syn keyword Function foldl foldr
syn keyword Function revfilter filter
" base/base.opi
syn keyword Function zero? positive? negative? even? odd?
syn keyword Function match split


syn keyword opiKeyword let rec and or in return
syn region opiBegin matchgroup=opiKeyword start=/\<begin\>/ end=/\<end\>/ contains=TOP
syn keyword opiAssert assert
syn keyword opiSpecial commandline environment

syn keyword opiKeyword if unless when then else
syn keyword opiKeyword yield
syn keyword opiLazy lazy

syn match opiOperator /[-+=*/%><&|.][-+=*/%><&|.!]*/
syn match opiOperator /![-+=*/%><&|!.]\+/
syn match opiOperator /:\|\$/
syn keyword opiOperator is eq equal not

syn match opiDelimiter /[,;()]/

syn match opiUnit /(\s*)/

syn keyword opiNil nil
syn keyword opiBoolean true false
syn keyword opiConstant stdin stdout stderr

syn match opiLambda /\\/
syn match opiLambda /->/

syn keyword opiLoad load

syn match opiTableRef /#/ nextgroup=opiKey skipwhite skipnl
syn match opiKey /\k\+/ contained

syn match Comment /^#!.*$/
syn match Comment /--.*$/ contains=opiCommentLabel
syn region Comment start=/{-/ end=/-}/ skipnl skipwhite contains=opiCommentLabel
syn match opiCommentLabel /[A-Z]\w*:/ contained

" Integer with - + or nothing in front
syn match Number '\<\d\+'

" Floating point number with decimal no E or e
syn match Number '\<\d\+\.\d*'

" Floating point like number with E and no decimal point (+,-)
syn match Number '\<\d[[:digit:]]*[eE][\-+]\=\d\+'

" Floating point like number with E and decimal point (+,-)
syn match Number '\<\d[[:digit:]]*\.\d*[eE][\-+]\=\d\+'

" String
" "..."
syn region String start=/"/ skip=/\\"/ end=/"/ skipnl skipwhite contains=opiFormat
" qq[...]
syn region String matchgroup=opiQq start=/qq\[/ skip=/\\]/ end=/\]/ skipnl skipwhite contains=opiFormat
" qq(...)
syn region String matchgroup=opiQq start=/qq(/ skip=/\\)/ end=/)/ skipnl skipwhite contains=opiFormat
" qq{...}
syn region String matchgroup=opiQq start=/qq{/ skip=/\\}/ end=/}/ skipnl skipwhite contains=opiFormat
" qq/.../
syn region String matchgroup=opiQq start=+qq/+ skip=+\\/+ end=+/+ skipnl skipwhite contains=opiFormat
" qq|...|
syn region String matchgroup=opiQq start=+qq|+ skip=+\\|+ end=+|+ skipnl skipwhite contains=opiFormat
" qq+...+
syn region String matchgroup=opiQq start=/qq+/ skip=/\\+/ end=/+/ skipnl skipwhite contains=opiFormat

" Shell
syn region String matchgroup=opiOperator start=/`/ skip=/\\`/ end=/\`/ skipnl skipwhite contains=opiFormat

"RerEx
" qq[...]
syn region String matchgroup=opiQq start=/qr\[/ skip=/\\]/ end=/\]/ skipnl skipwhite contains=opiFormat
" qr(...)
syn region String matchgroup=opiQq start=/qr(/ skip=/\\)/ end=/)/ skipnl skipwhite contains=opiFormat
" qr{...}
syn region String matchgroup=opiQq start=/qr{/ skip=/\\}/ end=/}/ skipnl skipwhite contains=opiFormat
" qr/.../
syn region String matchgroup=opiQq start=+qr/+ skip=+\\/+ end=+/+ skipnl skipwhite contains=opiFormat
" qr|...|
syn region String matchgroup=opiQq start=+qr|+ skip=+\\|+ end=+|+ skipnl skipwhite contains=opiFormat
" qr+...+
syn region String matchgroup=opiQq start=/qr+/ skip=/\\+/ end=/+/ skipnl skipwhite contains=opiFormat

"Search Replace
" /../../
syn region String matchgroup=opiQq start="s[g]*/" skip=+\\/+ end=+/+ nextgroup=opiSrPattern1 skipnl skipwhite contains=opiFormat
syn region opiSrPattern1 start=+.+ matchgroup=opiQq skip=+\\/+ end=+/+ skipnl skipwhite contains=opiFormat contained
" |..|..|
syn region String matchgroup=opiQq start="s[g]*|" skip=+\\|+ end=+|+ nextgroup=opiSrPattern2 skipnl skipwhite contains=opiFormat
syn region opiSrPattern2 start=+.+ matchgroup=opiQq skip=+\\|+ end=+|+ skipnl skipwhite contains=opiFormat contained
" +..+..+
syn region String matchgroup=opiQq start="s[g]*+" skip=/\\+/ end=/+/ nextgroup=opiSrPattern3 skipnl skipwhite contains=opiFormat
syn region opiSrPattern3 start=+.+ matchgroup=opiQq skip=/\\+/ end=/+/ skipnl skipwhite contains=opiFormat contained

syn match  SpecialChar /\\\d\+/ containedin=opiSrPattern1,opiSrPattern2,opiSrPattern3 contained
hi link opiSrPattern1 String
hi link opiSrPattern2 String
hi link opiSrPattern3 String

" Inline expression
syn region opiFormat matchgroup=opiSpecial start=/%{/ end=/}/ contained contains=TOP

" Special characters
syn match opiSpecial /\\$/ containedin=String contained
syn match SpecialChar "\\a" containedin=String contained
syn match SpecialChar "\\b" containedin=String contained
syn match SpecialChar "\\e" containedin=String contained
syn match SpecialChar "\\f" containedin=String contained
syn match SpecialChar "\\n" containedin=String contained
syn match SpecialChar "\\r" containedin=String contained
syn match SpecialChar "\\t" containedin=String contained
syn match SpecialChar "\\v" containedin=String contained
syn match SpecialChar "\\?" containedin=String contained
syn match SpecialChar "\\%" containedin=String contained



hi link opiModule Define
hi link opiUse Define

"hi link opiStruct     StorageClass
"hi link opiStructName Type

hi link opiStruct     Structure
hi link opiStructName StorageClass

hi link opiSpecial Special

hi link opiKeyword Statement
hi link opiAssert Keyword
hi link opiLazy Keyword

hi link opiType Type
hi link opiDef Type
hi link opiLambda Operator
hi link opiOperator Operator
hi link opiLoad Include
hi link opiUnit PreProc
hi link opiDelimiter Delimiter
hi link opiConditional Conditional
hi link opiNil Constant
hi link opiBoolean Boolean
hi link opiConstant Constant
hi link opiSymbol Constant

hi link opiKey Identifiers
hi link opiTableRef SpecialChar

hi link opiCommentLabel Label

hi link opiQq Keyword

"hi link opiIdentifier Identifier
