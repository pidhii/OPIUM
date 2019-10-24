" Vim syntax file
" Language: opium
" Maintainer: Ivan Pidhurskyi

if exists("b:current_syntax")
  finish
endif

set iskeyword+=?,'
syn match opiIdentifier /\<[a-zA-Z_][a-zA-Z0-9_]*['?]?\>/
syn match opiSymbol /'[^ \t\n(){}\[\]'";,]\+/

syn keyword opiNamespace namespace nextgroup=opiNamespaceName skipwhite skipnl
syn match   opiNamespaceName /\w\+/ contained
syn keyword opiUse use as

syn keyword opiTrait trait nextgroup=opiTraitName skipwhite skipnl
syn keyword opiStruct struct nextgroup=opiStructName skipwhite skipnl
syn keyword opiImpl impl
syn match   opiStructName /\w\+/ contained
syn match   opiTraitName /\w\+/ contained

syn keyword opiType number string symbol pair function

syn keyword Function write display newline

syn keyword Function null? car cdr

syn keyword Statement let rec and in

syn keyword opiConditional if then else

syn match opiOperator /[-+=*/%><&|]\+/
syn match opiOperator /[$:]/
syn keyword opiOperator is eq equal not

syn match opiDelimiter /[,;(){}]/

syn match opiUnit /(\s*)/

syn keyword opiNil nil
syn keyword opiBoolean true false

syn match opiLambda /\\/
syn match opiLambda /->/

syn keyword opiLoad load
syn match opiNamespaceRef /\<\w\+::/he=e-2 contains=opiNamespaceDots
syn match opiNamespaceDots /::/

syn match opiSend /@/

syn match Comment /#.*$/

" Integer with - + or nothing in front
syn match Number '\<\d\+'
syn match Number '\<[-+]\d\+'

" Floating point number with decimal no E or e
syn match Number '\<[-+]\d\+\.\d*'

" Floating point like number with E and no decimal point (+,-)
syn match Number '\<[-+]\=\d[[:digit:]]*[eE][\-+]\=\d\+'
syn match Number '\<\d[[:digit:]]*[eE][\-+]\=\d\+'

" Floating point like number with E and decimal point (+,-)
syn match Number '[-+]\=\d[[:digit:]]*\.\d*[eE][\-+]\=\d\+'
syn match Number '\d[[:digit:]]*\.\d*[eE][\-+]\=\d\+'

syn match String /"\(\\.\|[^"\\]\)*"/


hi link opiNamespace     Define
"hi link opiNamespaceName Identifier
hi link opiNamespaceDots Operator
"hi link opiNamespaceRef  Identifier
hi link opiUse Define

"hi link opiStruct     StorageClass
"hi link opiStructName Type

hi link opiStruct     Structure
hi link opiTrait      Structure
hi link opiImpl       Structure
hi link opiStructName StorageClass
hi link opiTraitName  StorageClass

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
hi link opiSend Statement
hi link opiSymbol Constant
