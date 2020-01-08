# opium
Pure (ignoring IO) functional scripting language with eager evaluation.

Note: Current implementation is only guaranteed to work with x86-64 GNU/Linux systems.

## Reference counting
Memory management in Opium is implemented with bare reference counting. It disables explicit mutable operations, but it is Ok for functional language, since these operations are not allowed anyway.
Also it enables implicit mutable operations on data structures  when reference count reaches zero (i.e. distructive copying).
It also has good effect on memory usage, yet "real-timeness" is arguable.

## Functions and application.
All functions are first-class objects. Basic syntax to define a functions is:
```
fn <arguments> -> <body>
```
or
```
\<arguments> -> <body>
```
These forms will create anonymous functions capturing outer variables by upvalue.

### Recursive functions
Recursive functions are defined with `let rec` form:
```
let rec <name> <arguments> = <body>
[and <another-name> <arguments> = <body>]
...
```
note that function definition as above, i.e., `<name> <arguments> = <body>` is equivalent to `<name> = fn <arguments> -> <body>`.
Functions defined withing `let rec` construct are able to reference each other. Consider common example of `even?` and `odd?`:
```
let rec even? x = x == 0 || odd? (x - 1)
and odd? x = x != 0 && even? (x - 1)
```

Functions of variable arguments can be defined using `..` before the last argument in signature.
Values passed to this "variadic argument" will be formed into a Cons-list. Example:
```
let foo first ..rest =
  print "first:" first;
  print "rest:" rest;
in foo 1 2 3 4 5
```
will output
```
first: 1
rest: 2:3:4:5:nil
```
Note that currently variadic functions may not be defined with `let rec`.

### Currying
All function applications are targets for currying by default, so no special syntax is needed.
I believe you are already familiar with concept of currying, yet one remark should be for variadic functions: currying is not implemented for them, so
```
let foo x y z = x + y + z
in foo x y
```
is Ok, but
```
let foo x y z ..rest = x + y + z + foldl (+) 0 rest
in foo 1 2
```
will raise a runtime error.

### Application operators
There are two auxilary operators for application:
* `$`-operator derived from Haskell
* `|>` (i.e. right pipe) operator derived from OCaml's batteries.

Note that `$` has higher precedence than `|>`.  
All following expressions are equivalent, and will print odd numbers in range of [0, 5):
* `foreach print $ filter odd? $ range 0 5`
* `range 0 5 |> filter odd? |> foreach print`
* `range 0 5 |> foreach print $ filter odd?`

## Structs
New data types may be defined with the `struct`-form:
```
struct <Name> { <fields> }
```
where `<fields>` reffer to a comma-separated list of structure members.
Note that first letter of a name MUST be in upper case.

### Constructors
There are three ways to create an instance of a struct-type.

#### Function-flavoured constructor
First way to create an instance is to directly use a generated constructor (which is just a regular function).
Example:
```
struct Foo { a, b }
let foo = Foo 1 2 in
print "foo:" foo;
```
output:
```
foo: Foo { a = 1, b = 2 }
```

#### Explicitly reference fields by name

#### Copy-constructor

## Traits

## Modules (namespaces)

## Numbers
Numbers are represented with extended precision floats, `long double` in C, which are implemented as either 80-bit or 128-bit IEEE floats.
This representation is capable of covering range of "exact integers" higher that of `size_t`, and, of course, introduces higher precision for arithmetics in general.

## Strings
Implementation of strings is similar to Lua's approach, i.e. string may contain null-bytes. Although terminating null byte is also required, so that string may be safly used for C FFI.
Unlike Lua, two strings of a same content are not guaranteed to be represented by the same object; it enables mutable operations under the hood when allowed.

## Symbols

## Tables
