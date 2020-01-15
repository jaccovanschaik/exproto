exproto
=======

Exproto is a prototype extractor for C code.

It was inspired by a program called cextract, written by Adam Bryant,
which had the same basic function, although with a lot more features.
Unfortunately cextract choked on a number of C code files, so I decided
to try and make a replacement. The result is exproto.

Exproto has the following usage information:

    Usage: exproto <options> [ <input-file> ]
    
    Extracts prototypes from C files.
    
    Options:
        -h --help		Show this help.
        -o --output <file>	Send output to this file.
        -p --cpp		Run cpp to pre-process source files.
        -c --comments		Include function comments in output.
        -s --statics		Include static functions.
  
    All other options are passed on as-is to cpp (if it is run).
    If <input-file> is not given or if it is '-', input is read from stdin.

Exproto extracts function signatures from the input file and writes them
to the standard output, or to the file given with the -o/--output option.

When the -p/--cpp option is given, the code is passed through the C
preprocessor first, which has the advantage that code that is ifdef'ed
out will also not appear in the output. When the -p/--cpp option is
*not* given, all preprocessor statements are ignored.

If you use -p/--cpp you may also have to give one or more -I or -D
options, depending on the include files and ifdefs you've used. All
options *other* than the ones given in the usage above are passed on to
the preprocessor, so it can find the included files.

If you give the -c/--comment option, the last comment before each
function definition will be reproduced in the output. This way,
functions get their function comments in the output as well. (This was
the killer feature from cextract that I absolutely wanted to have)

If you give the -s/--statics option, static functions in the input will
be reproduced in the output. Otherwise they will be filtered out.

Exproto is a very simple program. It works on the assumption that at the
top level of a C file, we will only encounter three things: comments
(block or line), preprocessor statements, and C declarations. We simply
reproduce those declarations that seem to define functions (i.e. the
ones that contain parentheses) and leave out the rest. If a declaration
is `static` we leave it in or take it out based on the -s/--statics
option.

There are some complications of course. We need to skip function bodies,
which means we have to count open and close curly braces, but we don't
want to count braces when they occur in a literal string or a comment.
And if we want to skip strings when counting braces, we must also deal
with embedded, escaped quote characters that don't actually end the
string.

All I'm saying is, if you came here expecting a full C parser you are
going to be very disappointed.

Exproto requires [libjvs](https://github.com/jaccovanschaik/libjvs).

To build exproto, edit the Makefile and set the needed variables. You
probably only need to change `JVS_TOP` to the prefix of your
installation of libjvs, and `INSTALL_TOP` to the prefix where you want
to install exproto. After that it's just `make` and `make install`.

Have fun!
