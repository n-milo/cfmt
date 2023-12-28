#ifndef CFMT_FORMAT_H
#define CFMT_FORMAT_H

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

// Type of custom formatter functions.
//
// Will only ever be called with one vararg, and its type will be exactly the
// type registered in CFMT_CUSTOM_PRINT_TYPES (with no indirection).
//
// Must write at most `size-1` characters (with the `size`th character being the
// terminating 0) to `buf` and return how many character would be written if
// `size` were unlimited.
// (essentially, behave exactly like snprintf)
typedef int printf_formatter_t(char *buf, size_t size, ...);

int unknown_formatter(char *buf, size_t size, ...);

struct printer_state {
    const char *fmt;

    // points to the null terminator of the writing buffer
    // if NULL, union is a file
    char *buf_end;

    union {
        FILE *file;
        char *buf;
    };

    struct printer_item *head, *tail;
};

struct printer_item {
    struct printer_item *next;
    const char *typename;
    char text[];
};

struct unprintable {}; // don't print this

#define print(format, ...)                                                              \
    PRINT_BEGIN(format)                                                                 \
    s.file = stdout;                                                                    \
    MAP(PRINT_PUSH, __VA_ARGS__)                                                        \
    PRINT_END()

#define fprint(f, format, ...)                                                          \
    PRINT_BEGIN(format)                                                                 \
    s.file = f;                                                                         \
    MAP(PRINT_PUSH, __VA_ARGS__)                                                        \
    PRINT_END()

#define snprint(str, size, format, ...)                                                 \
    PRINT_BEGIN(format)                                                                 \
    if (size == 0)                                                                      \
        break;                                                                          \
    s.buf = str;                                                                        \
    s.buf_end = str + size - 1;                                                         \
    MAP(PRINT_PUSH, __VA_ARGS__)                                                        \
    PRINT_END()

#define PRINT_BEGIN(format) do {                                                        \
    struct printer_state s = {0};                                                       \
    s.fmt = format;                                                                     \

#define PRINT_END()                                                                     \
    print_end(&s);                                                                      \
} while(0)

#define PRINT_DEFAULT_TYPES(X)                                                          \
        X(char, "%c")                                                                   \
        X(short, "%hd")                                                                 \
        X(int, "%d")                                                                    \
        X(long, "%ld")                                                                  \
        X(long long, "%lld")                                                            \
        X(unsigned int, "%u")                                                           \
        X(unsigned short, "%hu")                                                        \
        X(unsigned long, "%lu")                                                         \
        X(unsigned long long, "%llu")                                                   \
        X(char*, "%s")                                                                  \
        X(void*, "%p")                                                                  \
        X(float, "%g")                                                                  \
        X(double, "%g")                                                                 \
        X(long double, "%g")

#define PRINT_FMT_STR_X(T, fmt)                                                         \
    T: fmt,

#define PRINT_FMT_STR(x) _Generic((x),                                                  \
        PRINT_DEFAULT_TYPES(PRINT_FMT_STR_X)                                            \
        default: NULL                                                                   \
)

#ifndef CFMT_CUSTOM_PRINT_TYPES
#define CFMT_CUSTOM_PRINT_TYPES(X)
#endif

#define PRINT_FMT_FUNC_X(T, func)                                                       \
    T: func,

#define PRINT_FMT_FUNC(x) _Generic((x),                                                 \
        CFMT_CUSTOM_PRINT_TYPES(PRINT_FMT_FUNC_X)                                       \
        default: unknown_formatter                                                      \
)

#define PRINT_TYPE_NAME_X(T, x)                                                         \
    T: #T,

#define PRINT_TYPE_NAME(x) _Generic((x),                                                \
        PRINT_DEFAULT_TYPES(PRINT_TYPE_NAME_X)                                          \
        CFMT_CUSTOM_PRINT_TYPES(PRINT_TYPE_NAME_X)                                      \
        struct unprintable: "unknown"                                                   \
)

#define PRINT_PUSH(x) {                                                                 \
    const char *fmt = PRINT_FMT_STR(x);                                                 \
    size_t len;                                                                         \
    printf_formatter_t *formatter;                                                      \
    if (fmt) {                                                                          \
        len = (size_t) snprintf(NULL, 0, fmt, x);                                       \
    } else {                                                                            \
        formatter = PRINT_FMT_FUNC(x);                                                  \
        len = (size_t) formatter(NULL, 0, x);                                           \
    }                                                                                   \
    struct printer_item *item = malloc(sizeof(struct printer_item) + len + 1);          \
    item->next = NULL;                                                                  \
    item->typename = PRINT_TYPE_NAME(x);                                                \
    if (fmt) {                                                                          \
        snprintf(item->text, len + 1, fmt, x);                                          \
    } else {                                                                            \
        formatter(item->text, len + 1, x);                                              \
    }                                                                                   \
    if (s.tail)                                                                         \
        s.tail->next = item;                                                            \
    if (!s.head)                                                                        \
        s.head = item;                                                                  \
    s.tail = item;                                                                      \
}

void print_end(struct printer_state *s);

#ifdef CFMT_FORMAT_IMPLEMENTATION

#include <string.h>
#include <assert.h>

int unknown_formatter(char *buf, size_t size, ...) {
    return snprintf(buf, size, "%%!UNKNOWN");
}

// if `len` == 0 then `text` is null-terminated
static inline void print_write(struct printer_state *s, const char *text, size_t len) {
    if (s->buf_end) {
        if (len == 0)
            len = strlen(text);
        assert(s->buf_end > s->buf);
        size_t n = (size_t) (s->buf_end - s->buf - 1);
        if (n > len)
            n = len;
        memcpy(s->buf, text, n);
        s->buf += n;
        assert(s->buf_end > s->buf);
    } else {
        if (len) {
            fwrite(text, 1, len, s->file);
        } else {
            fputs(text, s->file);
        }
    }
}

void print_end(struct printer_state *s) {
    struct printer_item *i = s->head, *tmp;

    for (const char *c = s->fmt; *c; c++) {
        if (*c == '%') {
            if (i) {
                print_write(s, i->text, 0);
                tmp = i;
                i = i->next;
                free(tmp);
            } else {
                print_write(s, "%!MISSING", 0);
            }
        } else {
            print_write(s, c, 1);
        }
    }

    if (i) {
        print_write(s, "%!(EXTRA ", 0);
        while (i) {
            print_write(s, i->typename, 0);
            print_write(s, "=", 0);
            print_write(s, i->text, 0);
            tmp = i;
            i = i->next;
            free(tmp);
            if (i)
                print_write(s, ", ", 0);
        }
        print_write(s, ")", 0);
    }

    if (s->buf_end)
        *s->buf_end = 0;
}

#endif // CFMT_FORMAT_IMPLEMENTATION

#define EVAL0(...) __VA_ARGS__
#define EVAL1(...) EVAL0(EVAL0(EVAL0(__VA_ARGS__)))
#define EVAL2(...) EVAL1(EVAL1(EVAL1(__VA_ARGS__)))
#define EVAL3(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))
#define EVAL4(...) EVAL3(EVAL3(EVAL3(__VA_ARGS__)))
#define EVAL(...)  EVAL4(EVAL4(EVAL4(__VA_ARGS__)))

#define MAP_END(...)
#define MAP_OUT
#define MAP_COMMA ,

#define MAP_GET_END2() 0, MAP_END
#define MAP_GET_END1(...) MAP_GET_END2
#define MAP_GET_END(...) MAP_GET_END1
#define MAP_NEXT0(test, next, ...) next MAP_OUT
#define MAP_NEXT1(test, next) MAP_NEXT0(test, next, 0)
#define MAP_NEXT(test, next)  MAP_NEXT1(MAP_GET_END test, next)

#define MAP0(f, x, peek, ...) f(x) MAP_NEXT(peek, MAP1)(f, peek, __VA_ARGS__)
#define MAP1(f, x, peek, ...) f(x) MAP_NEXT(peek, MAP0)(f, peek, __VA_ARGS__)

#define MAP_LIST_NEXT1(test, next) MAP_NEXT0(test, MAP_COMMA next, 0)
#define MAP_LIST_NEXT(test, next)  MAP_LIST_NEXT1(MAP_GET_END test, next)

#define MAP_LIST0(f, x, peek, ...)                                                      \
    f(x) MAP_LIST_NEXT(peek, MAP_LIST1)(f, peek, __VA_ARGS__)
#define MAP_LIST1(f, x, peek, ...)                                                      \
    f(x) MAP_LIST_NEXT(peek, MAP_LIST0)(f, peek, __VA_ARGS__)

/**
 * Applies the function macro `f` to each of the remaining parameters.
 */
#define MAP(f, ...) EVAL(MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters and
 * inserts commas between the results.
 */
#define MAP_LIST(f, ...)                                                                \
    EVAL(MAP_LIST1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

#endif // CFMT_FORMAT_H
