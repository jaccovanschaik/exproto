/*
 * exproto.c: Prototype extractor.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: exproto.c 20 2020-01-13 13:22:33Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>

#include <libjvs/buffer.h>
#include <libjvs/defs.h>

static bool include_comment = FALSE;
static bool include_static_functions  = FALSE;
static bool use_cpp = FALSE;

/*
 * Read a string and add it to <buf>. The string should be terminated by <terminator>.
 */
static int handle_string(FILE *fp, Buffer *buffer, const char terminator)
{
    int c;

    while ((c = fgetc(fp)) != EOF && c != terminator) {
        bufAddC(buffer, c);

        /* Any escape sequence (particularly escaped quotes) should be entered as-is. */

        if (c == '\\') {
            c = fgetc(fp);
            bufAddC(buffer, c);
        }
    }

    return 0;
}

/*
 * Handle a preprocessor line. Extract and return filename if it is a linemarker line.
 */
static int handle_preprocessor_line(FILE *fp, char **filename)
{
    int c, line;

    if (fscanf(fp, "%d", &line) == 1) {
        /* Seems to be a line marker. Try to find a filename. */

        while ((c = fgetc(fp)) != EOF && c != '"' && c != '\n');

        if (c == '"') {
            Buffer buffer = { 0 };

            handle_string(fp, &buffer, '"');
            free(*filename);
            *filename = bufDetach(&buffer);

            bufReset(&buffer);
        }
    }

    /* Consume the rest of the line. */

    do {
        c = fgetc(fp);
    } while (c != EOF && c != '\n');

    return 0;
}

/*
 * Read a block comment and add it to <buffer>. The starting slash-star are already in <buf>.
 */
static int handle_block_comment(FILE *fp, Buffer *buffer)
{
    int c;

    while ((c = fgetc(fp)) != EOF) {
        bufAddC(buffer, c);

        if (c == '*') {
            c = fgetc(fp);

            if (c == '/') {
                bufAddC(buffer, c);
                break;
            }
            else {
                ungetc(c, fp);
            }
        }
    }

    return 0;
}

/*
 * Read a line comment and add it to <buf>. The leading slash-slash are already in <buf>.
 */
static int handle_line_comment(FILE *fp, Buffer *buf)
{
    int c;

    while ((c = fgetc(fp)) != EOF && c != '\n') {
        bufAddC(buf, c);
    }

    if (c == '\n') {
        bufAddC(buf, c);
    }

    return 0;
}

/*
 * Read a comment and add it to <buffer>, which already contains the first slash.
 */
static int handle_comment(FILE *fp, Buffer *buffer)
{
    int c = fgetc(fp);

    if (c == '*') {
        bufAddC(buffer, c);
        return handle_block_comment(fp, buffer);
    }
    else if (c == '/') {
        bufAddC(buffer, c);
        return handle_line_comment(fp, buffer);
    }
    else {
        ungetc(c, fp);
        return 0;
    }
}

/*
 * Read a compound statement and add it to <buffer>.
 */
static int handle_compound(FILE *fp, Buffer *buffer)
{
    int c;

    while ((c = fgetc(fp)) != EOF) {
        bufAddC(buffer, c);

        if (c == '}')
            break;
        else if (c == '"' || c == '\'')
            handle_string(fp, buffer, c);
        else if (c == '{')
            handle_compound(fp, buffer);
        else if (c == '/') {
            bufAddC(buffer, c);
            handle_comment(fp, buffer);
        }
    }

    return 0;
}

/*
 * Read a declaration up to a semicolon or an open curly brace and add it to <declaration>.
 */
const static
int
handle_declaration(FILE *fp, Buffer *declaration)
{
    int c;

    while ((c = fgetc(fp)) != EOF) {
        if (c == ';') {
            bufAddC(declaration, c);
            break;
        }
        else if (c == '{') {
            Buffer body = { 0 };
            handle_compound(fp, &body);
            bufReset(&body);
            break;
        }
        else if (c == '/') {
            bufAddC(declaration, c);
            handle_comment(fp, declaration);
        }
        else if (c == '"' || c == '\'') {
            handle_string(fp, declaration, c);
        }
        else {
            bufAddC(declaration, c);
        }
    }

    return 0;
}

/*
 * Process input from <in> and write the result to <out>. The name of the original file from which
 * prototypes are to be extracted is in <input>.
 */
static int process(const char *input, FILE *in, FILE *out)
{
    char *current_file = strdup(input);
    Buffer comment = { 0 }, declaration = { 0 };

    int c;

    while ((c = fgetc(in)) != EOF) {
        if (c == '#') {
            handle_preprocessor_line(in, &current_file);
        }
        else if (c == '/') {
            bufSetC(&comment, c);

            handle_comment(in, &comment);
        }
        else if (!isspace(c) && c != ';') {
            bufSetC(&declaration, c);

            handle_declaration(in, &declaration);

            if (strchr(bufGet(&declaration), '(') != NULL && strcmp(current_file, input) == 0) {
                const char *str;
                int len;

                // Trim all leading whitespace
                while (TRUE) {
                    str = bufGet(&declaration);
                    len = bufLen(&declaration);

                    if (isspace(str[0]))
                        bufTrim(&declaration, 1, 0);
                    else
                        break;
                }

                // Trim all trailing whitespace
                while (TRUE) {
                    str = bufGet(&declaration);
                    len = bufLen(&declaration);

                    if (isspace(str[len - 1]))
                        bufTrim(&declaration, 0, 1);
                    else
                        break;
                }

                const char *ptr = strstr(str, "static");
                bool include_this_function = false;

                if (ptr == NULL || include_static_functions)
                    include_this_function = true;   // No "static" or static functions are allowed
                else if (ptr == str && isspace(ptr[6]))
                    include_this_function = false;  // "static" at start, followed by whitespace
                else if (isspace(ptr[-1]) && isspace(ptr[6]))
                    include_this_function = false;  // "static" preceded and followed by whitespace
                else
                    include_this_function = true;   // "static" somewhere in the name maybe?

                if (include_this_function) {
                    fputc('\n', out);

                    if (include_comment && bufLen(&comment) > 0) {
                        fputs(bufGet(&comment), out);
                        fputc('\n', out);
                    }

                    fputs(str, out);

                    if (str[len - 1] != ';') fputc(';', out);

                    fputc('\n', out);
                }
            }

            bufClear(&comment);
        }
    }

    bufReset(&comment);
    bufReset(&declaration);

    free(current_file);

    return 0;
}

/*
 * Show usage information.
 */
static void usage(const char *msg, const char *argv0, int exitcode)
{
    if (msg != NULL) {
        fputs(msg, stderr);
        fputs("\n", stderr);
    }

    fprintf(stderr, "Usage: %s <options> [ <input-file> ]\n\n", argv0);

    fprintf(stderr, "Extracts prototypes from C files.\n\n");

    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h --help\t\tShow this help.\n");
    fprintf(stderr, "  -o --output <file>\tSend output to this file.\n");
    fprintf(stderr, "  -p --cpp\t\tRun cpp to pre-process source files.\n");
    fprintf(stderr, "  -c --comments\t\tInclude function comments in output.\n");
    fprintf(stderr, "  -s --statics\t\tInclude static functions.\n\n");
    fprintf(stderr, "All other options are passed on as-is to cpp (if it is run).\n");
    fprintf(stderr, "If <input-file> is not given or if it is '-', input is read from stdin.\n");

    exit(exitcode);
}

int main(int argc, char *argv[])
{
    FILE *in_fp = NULL, *out_fp = NULL;

    int i, r;

    char *input = NULL, *output = NULL;

    Buffer cmd = { 0 };

    bufSetS(&cmd, "cpp -C");

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            input = "-";
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--cpp") == 0) {
            use_cpp = TRUE;
        }
        else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            output = argv[++i];
        }
        else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--comments") == 0) {
            include_comment = TRUE;
        }
        else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--statics") == 0) {
            include_static_functions = TRUE;
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(NULL, argv[0], 0);
        }
        else if (argv[i][0] == '-') {
            bufAddC(&cmd, ' ');
            bufAddS(&cmd, argv[i]);
        }
        else if (input != NULL) {
            usage("Multiple input files specified\n", argv[0], -1);
        }
        else {
            input = argv[i];
        }
    }

    if (use_cpp) {
        if (input != NULL && strcmp(input, "-") != 0) {
            bufAddC(&cmd, ' ');
            bufAddS(&cmd, input);
        }
        else {
            input = "<stdin>";
        }

        if ((in_fp = popen(bufGet(&cmd), "r")) == NULL) {
            perror("popen");
            exit(1);
        }
    }
    else {
        if (input == NULL || strcmp(input, "-") == 0) {
            in_fp = stdin;
            input = "<stdin>";
        }
        else if ((in_fp = fopen(input, "r")) == NULL) {
            perror(input);
            exit(1);
        }
    }

    bufReset(&cmd);

    if (output == NULL) {
        out_fp = stdout;
    }
    else if ((out_fp = fopen(output, "w")) == NULL) {
        perror(output);
        exit(1);
    }

    r = process(input, in_fp, out_fp);

    if (out_fp != stdout)
        fclose(out_fp);

    if (use_cpp)
        pclose(in_fp);
    else if (in_fp != stdin)
        fclose(in_fp);

    return r;
}
