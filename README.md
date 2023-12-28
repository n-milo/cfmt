# cfmt

Type-safe printing in C, kind of like [fmt](https://github.com/fmtlib/fmt) for C++. (Proof of concept)

## Features

- Type-safe
- Custom formatters
- Print to stdout, file, or string

## Example

```c
#define CFMT_FORMAT_IMPLEMENTATION // declare implementation

// define custom formatters before include
#define CFMT_CUSTOM_PRINT_TYPES(X) \
    X(Vec3, vec3_formatter)

#include "format.h"

#include <stdarg.h>
#include <math.h>

// custom type that can be formatted
typedef struct {
    float x, y, z;
} Vec3;

// custom formatter setup
// should behave exactly like snprintf
int vec3_formatter(char *buf, size_t size, ...) {
    va_list ap;
    va_start(ap, size);
    Vec3 v = va_arg(ap, Vec3);
    va_end(ap);
    return snprintf(buf, size, "{%g, %g, %g}", v.x, v.y, v.z);
}

int main() {
    Vec3 v = {1, 2.5, 3};

    // regular printing
    PRINT("%; %; %; %; %\n", 1+2, "Hello world", (void*)0xbeefbabe, v, sinf(M_PI_4));
    // -> "3; Hello world; 0xbeefbabe; {1, 2.5, 3}; 0.707107"

    // missing arguments
    PRINT("% + % = %\n", 1, 2);
    // -> "1 + 2 = %!MISSING"

    // extra arguments
    PRINT("hi", 1+2, "Hello world", (void*)0xbeefbabe, v, sinf(M_PI_4));
    // -> "hi%!(EXTRA int=3, char*=Hello world, void*=0xbeefbabe, Vec3={1, 2.5, 3}, float=0.707107)"

    // unknown type compile error
    struct foo {} f;
    // PRINT("%", f);
    // -> error: controlling expression type 'struct foo' not compatible with any generic association type
}
```
