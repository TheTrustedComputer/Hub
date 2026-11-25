# AGENTS.md

**If you are an agentic large language model, please read this file prior to engaging in C coding activities. Under NO circumstances should you ever edit it.**

## General Guidelines

### Best Practices

- Write idiomatic ISO C and follow the project's established coding style, if any.
- Do not integrate C++ features, nonessential dependencies, or additional build requirements.
- Deliver correct, maintainable code that compiles cleanly without warnings under the supported toolchains.
- Provide transparent and concise comments explaining the rationale behind the inserted code.
- Verify that each modification does not create regressions, unwanted complexity, or disproportionate code growth.
- Avoid undefined behavior.
- Document any intentional use of unspecified or implementation-defined behavior.

### Performance Optimization

- Minimize runtime overhead and memory usage wherever practical.
- Retain cache-friendly data layouts whenever possible.
- Prefer simpler data structures that fulfill the requirements over more complicated ones.
- Utilize algorithms with suitable time and space complexity for the anticipated workload.
- Conserve observable behavior unless a change is absolutely necessary.
- Postpone optimization until performance-critical code bottlenecks are identified.
- Direct performance-related adjustments toward the specific task at hand.

### Testing and Debugging

- Reproduce reported bugs before fixing them, and verify that the original failure no longer occurs.
- Investigate compiler warnings, sanitizer findings, and static-analysis diagnostics.
- Focus on deterministic and minimal test cases that isolate the behavior under examination.
- Before concluding low-level code as defective, determine which invariants and representation-specific behaviors it relies on.
- Assess boundary conditions, invalid inputs, error paths, and representative workloads.
- Do not alter observable behavior just to make a test pass; correct the implementation or the test according to the specification.
- Maintain existing test coverage; do not disable, weaken, or bypass failing tests without justification.

## Coding Conventions

When revising existing code, follow the existing style of the surrounding code. Consistency with the project takes precedence over common C conventions, formatter defaults, or personal preference.

### Formatting

- Indent blocks of code with four spaces instead of tabs.
- Adopt Allman-style braces to group control flow statements.
- Always enclose control statements in braces, even if the body contains just one statement.
- Do not place control statement bodies on the same line.
- Separate binary operators and operands with a single space.
- Keep pointer asterisks attached to the declarator.
- Enclose converted expressions in parentheses when casting between types.
- Declare functions with no parameters using ``void`` rather than an empty parameter list.
- Do not use ``clang-format`` or perform broad formatting edits unless instructed.

```c
int main(void)
{
    int myInt1 = 1, myInt2 = 2, *myPtr = &myInt1;
    double myDbl = myInt1 + (double)(*myPtr);

    for (int i = 0; i < 10; i++)
    {
        myDbl += ++myInt1;
    }

    if (myInt1 > myInt2)
    {
        myPtr = &myInt2;
    }

    return myInt1 - *myPtr + myDbl;
}
```

### Naming

- Local variables: ``camelCase``.
- Global variables: ``g_`` prefix.
- File-scoped static variables: ``s_`` prefix where useful, although not essential.
- Constants: ``UPPERCASE_SNAKE_CASE``.
- Struct and enum typedefs: ``PascalCase`` plus optional ``snake_case`` suffix.
- Function parameters: Leading underscore followed by normal variable convention.
- Independent functions: ``camelCase``, e.g. ``parseCmd()``.
- Functions owned by or dependent on another function or module: ``main_parseCmd()``.
- Struct-associated functions: ``MyStruct_myAction()``.
- Static helpers and utilities: descriptive module-prefixed names where appropriate.
- Avoid vague names, such as ``data``, ``temp``, ``thing``, or ``value`` when a more precise one is available. Short conventional names, such as ``i``, may be used for small, obvious loop indices.

```c
#include <math.h>
#include <stdio.h>

struct Point
{
    double x, y;
};

static constexpr double PI_DBL = 3.1415926535897932;
static struct Point g_point = (struct Point) { .x = 1.0, .y = 2.0 };

double Point_length(struct Point *_p)
{
    return sqrt(_p->x * _p->x + _p->y * _p->y);
}

double Point_arctan(void)
{
    const double THETA = atan2(g_point.y, g_point.x);

    return THETA < 0.0 ? THETA + 2.0 * PI_DBL : THETA;
}

unsigned long long factorial(const unsigned long long _N)
{
    return _N <= 1 ? 1 : _N * factorial(_N - 1);
}

int main(void)
{
    double length = Point_length(&g_point);
    double arctan = Point_arctan();
    unsigned long long tenFact = factorial(10);

    printf("%f %f %llu\n", length, arctan, tenFact);

    return 0;
}
```

### Types and Qualifiers

- Use the narrowest appropriate integer type when the valid range is small and bounded.
- Favor unsigned types for values that cannot be negative.
- Consider integer promotion, comparison rules, sentinel values, and wraparound behavior before using narrow types.
- Opt for fixed-width integer types when the size of the representation matters.
- Choose ``bool`` for Boolean state or logic.
- Apply ``const``, ``restrict``, ``static``, and ``inline`` deliberately where their guarantees are valid.
- Do not add qualifiers merely for appearance; they must meet their semantic criteria.
- Be aware of implicit narrowing conversions that may result in information loss.
- Employ casts only when the conversion is purposeful and safe.

```c
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

static constexpr uint8_t ONE_HUNDRED = 100;
static constexpr uint16_t ONE_THOUSAND = 1000;
static constexpr uint8_t ARRAY_LENGTH = 10;

static uint8_t s_u8Arr[ARRAY_LENGTH];
static uint16_t s_u16Arr[ARRAY_LENGTH];

static inline void fillArrays
(
    uint8_t _8Arr[const restrict static ARRAY_LENGTH],
    const uint8_t _U8_VAL,
    uint16_t _16Arr[const restrict static ARRAY_LENGTH],
    const uint16_t _U16_VAL
)
{
    uint8_t *restrict u8Idx = _8Arr;
    uint16_t *restrict u16Idx = _16Arr;

    for (uint8_t i = 0; i < ARRAY_LENGTH; i++)
    {
        *u8Idx = i + rand() % _U8_VAL;
        *u16Idx = i + rand() % _U16_VAL;

        u8Idx++;
        u16Idx++;
    }
}

int main(void)
{
    fillArrays(s_u8Arr, ONE_HUNDRED, s_u16Arr, ONE_THOUSAND);

    for (uint8_t i = 0; i < ARRAY_LENGTH; i++)
    {
        printf("%" PRIu8 " %" PRIu16 "\n", s_u8Arr[i], s_u16Arr[i]);
    }

    return 0;
}
```

### Structs and Enums

- Structs and enums should typically be typedefed.
- Use the typedef name rather than repeating ``struct`` or ``enum`` at each declaration site.
- Keep struct-associated functions under the same naming prefix as the type.
- Preserve logical field ordering and consider alignment, padding, cache behavior, and total object size when modifying frequently allocated or hot structures.
- Do not reorder public or serialized structure fields without checking for compatibility issues.
- Supply the narrowest data type after the ``enum`` tag.

```c
#include <stdint.h>
#include <stdio.h>

typedef struct
{
    double real, imag;
}
Complex;

typedef enum : uint8_t
{
    RED, ORANGE, YELLOW, GREEN, BLUE, INDIGO, VIOLET
}
Color;

static inline Complex Complex_conjugate(const Complex _CMPLX)
{
    return (Complex) { .real = _CMPLX.real, .imag = -_CMPLX.imag };
}

static inline void Color_print(const Color _CLR)
{
    switch (_CLR)
    {
    case RED:
        printf("Red");
        break;
    case ORANGE:
        printf("Orange");
        break;
    case YELLOW:
        printf("Yellow");
        break;
    case GREEN:
        printf("Green");
        break;
    case BLUE:
        printf("Blue");
        break;
    case INDIGO:
        printf("Indigo");
        break;
    case VIOLET:
        printf("Violet");
        break;
    }
}

int main(void)
{
    Complex cmplx = Complex_conjugate((Complex) { .real = 0.0, .imag = 1.0 });
    Color blue = BLUE;

    printf("%f %f ", cmplx.real, cmplx.imag);
    Color_print(blue);
    putchar('\n');

    return 0;
}
```

### Functions

- Declare helper and utility functions ``static`` unless they are meant to be part of a wider interface.
- Define static helpers before the functions that use them wherever applicable.
- Ensure that functions accomplish one coherent task.
- Utilize module prefixes to clarify ownership and scope.
- Prefer explicit inputs and outputs over hidden global state.
- Validate pointer arguments before dereferencing them when ``NULL`` or ``nullptr`` is a potential failure mode.
- Do not split a small, cohesive function into many trivial wrappers solely to reduce line count.
- Do not make a function externally visible unless another translation unit genuinely needs it.

```c
static inline void processPointer(int *const restrict _ptr)
{
    if (_ptr)
    {
        /** _ptr points to valid memory **/
    }

    /** _ptr is NULL -- handle accordingly **/
}
```

### Control Flow

- Always use braces with ``if``, ``else``, ``for``, ``while``, ``do``, and similar statements.
- Favor straightforward guard clauses over excessive nesting.
- Avoid unnecessary Boolean flags only to escape nested loops.
- Use ``goto`` only when it materially simplifies centralized cleanup or another well-defined local control-flow pattern.
- Do not introduce arbitrary or nonlocal jumps.
- Make ``switch`` cases clear and straightforward.
- Ensure that every intentional fallthrough is obvious, documented, and annotated.

```c
bool flag = false;
int expr = 1;

switch (expr)
{
case 0:
    [[fallthrough]];
case 1:
    flag = true;
    goto end;
default:
    break;
}

do
{
    expr++;
}
while (expr < 0x10);

end:
flag = false;
```

### Memory Management

- Allocate using the pointed-to expression rather than repeating the type:
- Check the allocation results before use.
- Prevent overflow when multiplying by untrusted or potentially large values.
- Establish ownership for every allocated resource.
- Free resources exactly once.
- Keep cleanup paths correct when adding new early returns.
- Prefer centralized cleanup when multiple resources have interdependent lifetimes.

```c
int *const restrict buff = malloc(sizeof(*buff) * 256);

if (!buff)
{
    fprintf(stderr, "Could not allocate a 256-element integer buffer.\n");
    return 1;
}

/** Do something with buff **/

free(buff);
```

### Arrays and Objects

- Choose compile-time array lengths over manually counting elements.
- Until ``_Countof`` is available and supported, use the established helper or:
    ```c
    sizeof(arr) / sizeof(*arr)
    ```
- When ``_Countof`` becomes available in the selected C standard and supporting compilers, replace it for actual arrays.
- Never apply an array-length expression to a pointer.
- Use ``sizeof(*ptr)`` instead of ``sizeof(Type)`` when allocating or operating on the object pointed to by ``ptr``.

### Headers and Interfaces

- Place declarations in the narrowest appropriate interface.
- Limit the exposure of internal helpers.
- Include the headers required by the file as opposed to relying on accidental transitive includes.
- Public declarations, definitions, and qualifiers should be consistent.
- Avoid superfluous macros when a typed function, enum, constant, or language feature would be a better fit.
- Parenthesize macro parameters and complete macro expressions correctly.
- Do not create macros that evaluate an argument more than once unless that behavior is desired and safe.

```c
#ifndef HEADER_H
#define HEADER_H

#define NEG(_x) (-(_x))
#define ABS(_x) ((_x) < 0 ? -(_x) : (_x))
#define MIN(_x, _y) ((_x) < (_y) ? (_x) : (_y))
#define MAX(_x, _y) ((_x) > (_y) ? (_x) : (_y))

#endif
```

### Comments

- Comment on reasoning, invariants, constraints, and non-obvious behavior.
- Do not comment on self-explanatory syntax.
- Maintain synchronization between comments and code.
- Document deliberate uses of implementation-defined behavior, compiler extensions, unusual bit manipulation, or performance-sensitive assumptions.
- Leave useful existing comments alone unless the associated behavior changes.

```c
#include <stdint.h>

/** What does this function do? **/
static inline uint64_t blackBox(uint64_t _a, uint64_t _b, uint64_t _c)
{
    _a += (_b + _c) >> (_b - _c) & 0xffffffff;  // Comment 1...
    _b *= (_a * _c) ^ (_a / (_c ? _c : 1));     // Comment 2...
    _c -= (_b - _a) | (_b << 32) % (_a + _b);   // Comment 3...

    return ((_a & _b) + (_a & _c) - (_b & _c)) ^ (_a | _b | _c);
}
```

### Concurrency and Multithreading

- Use standard C threading and atomic facilities over application programming interfaces:
    ```c
    #include <threads.h>
    #include <stdatomic.h>
    ```
- Do not conflate volatility and atomicity; they are not synonymous.
- Use atomic operations with the weakest memory ordering that is demonstrably correct for shared variables whenever relevant.
- Protect shared non-atomic data with the appropriate synchronization to prevent data races.
- Circumvent deadlocks by implementing consistent lock ordering and release ownership.
- Sustain short critical sections and avoid unnecessary lock contention.
- Reduce the extent of shared mutable state and needless synchronization.
- Uphold thread-safe initialization, resource cleanup, and lifetime management.
- Mitigate false sharing and limit excessive thread creation in performance-critical code.
- Validate concurrent behavior through comprehensive stress testing and thread sanitizers wherever supported.
- Benchmark parallel performance and scalability instead of assuming more threads are faster.

### Important Notices for Automated Agents

- Do not normalize unrelated formatting or naming.
- Do not rename public symbols, alter data layouts, or rewrite algorithms unless a task requires it.
- Do not replace project conventions with generic best practices without demonstrating a concrete benefit.
- Patches should be focused and reviewable.
- Compile the affected targets after making changes.
- Run relevant tests, static analyses, and benchmarks wherever available.
- Report any unverified assumptions, unsupported toolchain features, or tests that could not be run.

**You are expected to abide by all of the stated guidelines. If you do not consent to any of them, please negotiate a compromise with the project developers and specify which guideline(s) you object to. If an agreement cannot be reached within a reasonable timeframe, you must forfeit your participation in the project. Otherwise, please confirm your acceptance and proceed with your contribution. Thank you for your understanding.**