# opium
Pure (almost) functional scripting language.
On my opinion most scripting are too flexible. They allow you to do whatever you want, which is cool, but combined with a lack of strict type system you program often becomes a hardcoded mess. Another annoying corner of these languages are lazy garbage collectors. First thing I hate is the fact that you really can't have any idea of when your objects will be finaly deleted. Also, while lifetimes are not so crusial in reality, what is indeed important (on my opinion) is that these fucking GCs will pollute half of your RAM before finaly realizing that the time has come to clean up a bit. With these in mind, memory management in Opium is implemented on pure reference counting. Even though it still doesn't grant you an absolute confidence about lifetimes (in general), you RAM is free to be abused by any other soft you run on your PC.

## So what is opium?
In general, opium is a dynamic functional language with ML-like syntax, call-by-balue application, currying, [tail-call](#tail-calls) recursion, and no native form of mutable variables (but I can't prevent you from introducing mutable environment via C-library).

# Basic examples
I won't deepen into syntax too much here, and assume that you are familliar with traditional ML syntax and also able to detect the differences.

### Delimiting expressions
Top-level statements use different delimiting rules compared to statements inside some block-like statements, i.e:

![](/12-07-19-19:36:47.png)

Note: following code snippets are written in nested-block manner unless state otherwize.

### Chaining functions
Unlike F# and OCaml in opium you chain functions with `$`(application operator) and `.`(function composition) like in Haskell:
<pre><code>
-- read a column of lines from a file
map number . split qr/\n/ . chop . read $ open "file.txt";

-- ...BTW file is a sequence of lines by default, so above expression can be replaced by
map number $ open "file.txt";
</code></pre>

### Sequences
Like in Python, most functions operating on sequences return a sort of a generator which will yield elements of an output sequence on demand. Unlike in Python, in opium these lazy sequences are being reset each time they are reused, and thus they don't get draind by the first consummer it was supplied to:  
<pre><code>
<b>let</b> seq = map (\x -> x*x) [1 2 3 4 5] <b>in</b>
print "seq =" seq;

<b>let</b> l = list seq <b>in</b>
print "l =" l;

<b>let</b> l2 = list $ map (fn x -> x / 2) seq <b>in</b>
print "l2 =" l2;
</code></pre>
output:
```
seq = <Seq>
l = 1:4:9:16:25:nil
l2 = 0.5:2:4.5:8:12.5:nil
```

# Traits

# Regular Expressions

# Tail Calls
