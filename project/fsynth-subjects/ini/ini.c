/* inih -- simple .INI file parser

inih is released under the New BSD license (see LICENSE.txt). Go to the project
home page for more info:

https://github.com/benhoyt/inih

*/

/* 0  - Valid
 * -1 - incomplete
 *  1 - incorrect
 * */

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "ini.h"

#if !INI_USE_STACK
#include <stdlib.h>
#endif

#define MAX_SECTION 500
#define MAX_NAME 500

#ifdef INI_STOP_ON_FIRST_ERROR
#undef INI_STOP_ON_FIRST_ERROR
#endif

#define MAXFILESIZE (100 * 1024) // 100 KiB max file size

int linenos = 0;
char comment_after_incomplete = 0;

/* Used by ini_parse_string() to keep track of string parsing state. */
typedef struct {
    const char* ptr;
    size_t num_left;
} ini_parse_string_ctx;

/* Strip whitespace chars off end of given string, in place. Return s. */
static char* rstrip(char* s)
{
    char* p = s + strlen(s);
    while (p > s && isspace((unsigned char)(*--p)))
        *p = '\0';
    return s;
}

/* Return pointer to first non-whitespace char in given string. */
static char* lskip(const char* s)
{
    while (*s && isspace((unsigned char)(*s)))
        s++;
    return (char*)s;
}

/* Return pointer to first char (of chars) or inline comment in given string,
   or pointer to null at end of string if neither found. Inline comment must
   be prefixed by a whitespace character to register as a comment. */
static char* find_chars_or_comment(const char* s, const char* chars)
{
#if INI_ALLOW_INLINE_COMMENTS
    int was_space = 0;
    while (*s && (!chars || !strchr(chars, *s)) &&
           !(was_space && strchr(INI_INLINE_COMMENT_PREFIXES, *s))) {
        was_space = isspace((unsigned char)(*s));
        s++;
    }
#else
    while (*s && (!chars || !strchr(chars, *s))) {
        s++;
    }
#endif
    return (char*)s;
}

/* Version of strncpy that ensures dest (size bytes) is null-terminated. */
static char* strncpy0(char* dest, const char* src, size_t size)
{
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
    return dest;
}

/* See documentation in header file. */
int ini_parse_stream(ini_reader reader, void* stream, ini_handler handler,
                     void* user)
{
    linenos = 0;
    /* Uses a fair bit of stack (use heap instead if you need to) */
#if INI_USE_STACK
    char line[INI_MAX_LINE];
    int max_line = INI_MAX_LINE;
#else
    char* line;
    int max_line = INI_INITIAL_ALLOC;
#endif
#if INI_ALLOW_REALLOC && !INI_USE_STACK
    char* new_line;
    int offset;
#endif
    char section[MAX_SECTION] = "";
    char prev_name[MAX_NAME] = "";

    char* start;
    char* end;
    char* name;
    char* value;
    int error = 0;

#if !INI_USE_STACK
    line = (char*)malloc(INI_INITIAL_ALLOC);
    if (!line) {
        return -2;
    }
#endif

#if INI_HANDLER_LINENO
#define HANDLER(u, s, n, v) handler(u, s, n, v, lineno)
#else
#define HANDLER(u, s, n, v) handler(u, s, n, v)
#endif

    /* Scan through stream line by line */
    while (reader(line, max_line, stream) != NULL) {
#if INI_ALLOW_REALLOC && !INI_USE_STACK
        offset = strlen(line);
        while (offset == max_line - 1 && line[offset - 1] != '\n') {
            max_line *= 2;
            if (max_line > INI_MAX_LINE)
                max_line = INI_MAX_LINE;
            new_line = realloc(line, max_line);
            if (!new_line) {
                free(line);
                return -2;
            }
            line = new_line;
            if (reader(line + offset, max_line - offset, stream) == NULL)
                break;
            if (max_line >= INI_MAX_LINE)
                break;
            offset += strlen(line + offset);
        }
#endif

        linenos++;

        start = line;
#if INI_ALLOW_BOM
        if (linenos == 1 && (unsigned char)start[0] == 0xEF &&
                           (unsigned char)start[1] == 0xBB &&
                           (unsigned char)start[2] == 0xBF) {
            start += 3;
        }
#endif
        start = lskip(rstrip(start));

        if (strchr(INI_START_COMMENT_PREFIXES, *start)) {
            /* Start-of-line comment */
        }
#if INI_ALLOW_MULTILINE
        else if (*prev_name && *start && start > line) {
            /* Non-blank line with leading whitespace, treat as continuation
               of previous name's value (as per Python configparser). */
            if (!HANDLER(user, section, prev_name, start) && !error)
                error = linenos;
                // Valid line:
                printf("0");

        }
#endif
        else if (*start == '[') {
            /* A "[section]" line */
            end = find_chars_or_comment(start + 1, "]");
            if (*end == ']') {
                *end = '\0';
                strncpy0(section, start + 1, sizeof(section));
                *prev_name = '\0';
            }
            else if (!error) {
                end = find_chars_or_comment(start+1,INI_INLINE_COMMENT_PREFIXES);
                if (*end != '\0'){
                    comment_after_incomplete = 1;
                }
                /* No ']' found on section line */
                error = linenos;
                // Missing closing square bracket:
                printf("2");

            }
        }
        else if (*start) {
            /* Not a comment, must be a name[=:]value pair */
            end = find_chars_or_comment(start, "=:");
            if (*end == '=' || *end == ':') {
                *end = '\0';
                name = rstrip(start);
                value = end + 1;
#if INI_ALLOW_INLINE_COMMENTS
                end = find_chars_or_comment(value, NULL);
                if (*end)
                    *end = '\0';
#endif
                value = lskip(value);
                rstrip(value);

                /* Valid name[=:]value pair found, call handler */
                strncpy0(prev_name, name, sizeof(prev_name));
                if (!HANDLER(user, section, name, value) && !error)
                    error = linenos;
                    // Valid line:
                printf("0");
            }
            else if (!error) {
                /* No '=' or ':' found on name[=:]value line */
                end = find_chars_or_comment(start+1,INI_INLINE_COMMENT_PREFIXES);
                if (*end != '\0'){
                    comment_after_incomplete = 1;
                }

                error = linenos;
                // Invalid line:
                printf("4");
            }
        }

#if INI_STOP_ON_FIRST_ERROR
        if (error)
            break;
#endif
    }

#if !INI_USE_STACK
    free(line);
#endif
    // empty string or whitespace only:
    printf("0");
    return error;
}

/* See documentation in header file. */
int ini_parse_file(FILE* file, ini_handler handler, void* user)
{
    return ini_parse_stream((ini_reader)fgets, file, handler, user);
}

/* See documentation in header file. */
int ini_parse(const char* filename, ini_handler handler, void* user)
{
    FILE* file;
    int error;

    file = fopen(filename, "r");
    if (!file)
        return -1;
    error = ini_parse_file(file, handler, user);
    fclose(file);
    return error;
}

/* An ini_reader function to read the next line from a string buffer. This
   is the fgets() equivalent used by ini_parse_string(). */
static char* ini_reader_string(char* str, int num, void* stream) {
    ini_parse_string_ctx* ctx = (ini_parse_string_ctx*)stream;
    const char* ctx_ptr = ctx->ptr;
    size_t ctx_num_left = ctx->num_left;
    char* strp = str;
    char c;

    if (ctx_num_left == 0 || num < 2)
        return NULL;

    while (num > 1 && ctx_num_left != 0) {
        c = *ctx_ptr++;
        ctx_num_left--;
        *strp++ = c;
        if (c == '\n')
            break;
        num--;
    }

    *strp = '\0';
    ctx->ptr = ctx_ptr;
    ctx->num_left = ctx_num_left;
    return str;
}

/* See documentation in header file. */
int ini_parse_string(const char* string, ini_handler handler, void* user) {
    ini_parse_string_ctx ctx;

    ctx.ptr = string;
    ctx.num_left = strlen(string);
    return ini_parse_stream((ini_reader)ini_reader_string, &ctx, handler,
                            user);
}

// newly added for running the code
typedef struct
{
    int empty;
} configuration;

// took the example from the README file to have sth. to parse
static int handler(void* user, const char* section, const char* name,
                   const char* value)
{
    return 1;
}

FILE* v = 0;
char* read_input() {
    int counter = 0;
    char* chars = malloc(sizeof(char) * MAXFILESIZE);
    int c = 0;
    while((c = fgetc(v)) != EOF){
        if (counter == MAXFILESIZE) {
            exit(-1);
        }
        if (c == '\0'){
            c = 'X'; // Ugly workaround for dealing with 0 bytes in corrupted files
        }
        chars[counter++] = c;
    }
    chars[counter] = '\0';
    return chars;
}

int main(int argc, char** argv) {
    configuration config;
    if (argc > 1) {
      v = fopen(argv[1], "r");
    } else {
      v = stdin;
    }
    char* string = read_input();
    if (argc > 1) {
      fclose(v);
    }
    //printf(string);
    //int num = 999;
    //num = ini_parse_string(string, handler, &config);

    //char* str;
    //asprintf (&str, "%i", num);
    //printf(str);
    //printf("\n");
    //free(str);
    int actual_linenos = 1; // Ugly workaround for detecting empty lines at the end of the file
    for (char* c = string; *c; c++){
        if (*c == '\n'){
            actual_linenos++;
        }
    }

    int ret = ini_parse_string(string, handler, &config);
    printf("\nLineno %d/%d", ret, actual_linenos);
    if (ret > 0){
        if (ret >= actual_linenos){
            if (comment_after_incomplete){
                return 1; // Incomplete, but there was a comment afterwards
            } else {
                return -1; // Incomplete - Error occurred in last line
            }
        } else {
            return 1; // Incorrect - Error occurred somewhere in the middle of the file
        }
    } else if (ret < 0) {
        return 1; // File IO Error?
    }
    return 0; // Valid

}
