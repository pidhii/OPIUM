" Vim syntax file
" Language: opium
" Maintainer: Ivan Pidhurskyi

if exists("b:current_syntax")
  finish
endif

set comments=sr:{-,mb:-,ex:-}

set iskeyword+=?,'
syn match opiIdentifier /\<[a-zA-Z_][a-zA-Z0-9_]*['?]?\>/
syn match opiSymbol /'[^ \t\n(){}\[\]'";,:]\+/

syn keyword opiNamespace namespace nextgroup=opiNamespaceName skipwhite skipnl
syn match   opiNamespaceName /\k\+/ contained
syn keyword opiUse use as

syn keyword opiStruct struct nextgroup=opiStructName skipwhite skipnl
syn match   opiStructName /\k\+/ contained

syn keyword opiType  null  undefined  number  symbol  string  boolean  pair  fn FILE table
syn keyword Function null? undefined? number? symbol? string? boolean? pair? fn?

syn region opiList matchgroup=opiType start=/\[/ matchgroup=opiType end=/\]/ skipwhite skipnl contains=TOP

"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""'
" Builtins:
syn keyword Function write display newline print printf fprintf format
syn keyword Function car cdr list
syn keyword Function apply applylist vaarg
syn keyword Function id
syn keyword Function die error
syn keyword Function force
syn keyword Function system shell
syn keyword Function loadfile
syn keyword Function exit

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
" Base:
syn keyword Function length revappend
syn keyword Function strlen substr strstr chop chomp ltrim trim
syn keyword Function open popen
syn keyword Function getline getdelim
" base/common.opi
syn keyword Function flip
" base/list-base.opi
syn keyword Function range
syn keyword Function reverse
syn keyword Function any? all?
syn keyword Function revmap map
syn keyword Function foreach
syn keyword Function foldl foldr
syn keyword Function revfilter filter
" base/base.opi
syn keyword Function zero? positive? negative? even? odd?


syn keyword opiKeyword let rec and or in return begin end
syn keyword opiSpecial ARGV ENV

syn keyword opiKeyword if unless then else

syn match opiLazy /@/

syn match opiOperator /[-+=*/%><&|.][-+=*/%><&|.!]*/
syn match opiOperator /![-+=*/%><&|!.]\+/
syn match opiOperator /:\|\$/
syn keyword opiOperator is eq equal not

syn match opiDelimiter /[,;(){}]/

syn match opiUnit /(\s*)/

syn keyword opiNil nil
syn keyword opiBoolean true false
syn keyword opiConstant stdin stdout stderr

syn match opiLambda /\\/
syn match opiLambda /->/

syn keyword opiLoad load
syn match opiNamespaceRef /\<\w\+::/he=e-2 contains=opiNamespaceDots
syn match opiNamespaceDots /::/

syn match opiTableRef /#/ nextgroup=opiKey
syn match opiKey /\k\+/ contained

syn match Comment /^#!.*$/
syn match Comment /--.*$/ contains=Label
syn region Comment start=/{-/ end=/-}/ skipnl skipwhite contains=Label
syn match Label /[A-Z]\w*:/

" Integer with - + or nothing in front
syn match Number '\<\d\+'

" Floating point number with decimal no E or e
syn match Number '\<\d\+\.\d*'

" Floating point like number with E and no decimal point (+,-)
syn match Number '\<\d[[:digit:]]*[eE][\-+]\=\d\+'

" Floating point like number with E and decimal point (+,-)
syn match Number '\<\d[[:digit:]]*\.\d*[eE][\-+]\=\d\+'

syn region String start=/"/ skip=/\\.\|[^"\\]/ end=/"/ skipnl
syn match String /`\(\\.\|[^`\\]\)*`/hs=s+1,he=e-1 skipwhite skipnl
syn match Operator /`/ contained containedin=String


hi link opiNamespace     Define
"hi link opiNamespaceName Identifier
"hi link opiNamespaceDots Operator
"hi link opiNamespaceRef  Identifier
hi link opiUse Define

"hi link opiStruct     StorageClass
"hi link opiStructName Type

hi link opiStruct     Structure
hi link opiStructName StorageClass

hi link opiSpecial Special

hi link opiKeyword Conditional

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
hi link opiTableRef Keywords

