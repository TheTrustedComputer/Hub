# AGENTS.md

This repository contains the C source code for a performance-oriented Connect 4 solver called "Four the Win!" (FTW). Consult the ``README.md`` file for more information.

## Guidelines

### General Best Practices

- Write idiomatic ISO C and follow the project's established coding style.
- Do not introduce C++ features, nonessential dependencies, or additional build requirements.
- Produce correct, maintainable code that compiles cleanly without warnings under the supported toolchains.
- Provide clear and concise comments explaining the purpose of the inserted code.
- Verify that each modification does not create regressions, unwanted complexity, or excessive code growth.
- Avoid undefined behavior.
- Document any intentional use of unspecified or implementation-defined behavior.

### Performance Optimization

- Minimize runtime overhead and memory usage wherever practical.
- Retain cache-friendly data layouts whenever possible.
- Prefer simpler data structures that fulfill the requirements over more complicated ones.
- Utilize algorithms with suitable time and space complexity for the anticipated workload.
- Conserve observable behavior unless a change is absolutely necessary.
- Postpone optimization until performance-critical code bottlenecks are identified.
- Make performance-related adjustments relevant to the requested task.

### Testing and Debugging

- Reproduce reported bugs before fixing them, and verify that the original failure no longer occurs.
- Investigate compiler warnings, sanitizer findings, and static-analysis diagnostics instead of suppressing them without cause.
- Focus on deterministic and minimal test cases that isolate the behavior under examination.
- Before identifying low-level code as defective, determine which invariants and representation-specific behaviors it relies on.
- Test boundary conditions, invalid inputs, error paths, and representative workloads.
- Do not alter observable behavior just to make a test pass; correct the implementation or the test according to the specification.
- Maintain existing test coverage, and do not disable, weaken, or bypass failing tests without justification.

## Coding Conventions

Follow the existing style of the surrounding code. Consistency with the project takes precedence over common C conventions, formatter defaults, or personal preference.

### Formatting

- Indent blocks of code with four spaces instead of tabs.
- Use Allman-style braces.
- Always enclose control statements in braces, even if the body contains only one statement.
- Do not place control-statement bodies on the same line.
- Separate binary operators and operands with a single space.
- Keep pointer asterisks attached to the declarator:
    ```c
    int *ptrX, *ptrY;
    ```
- Write casts with parentheses around the converted expression:
    ```c
    (int)(value);
    ```
- Declare functions with no parameters using ``void`` rather than an empty parameter list:
    ```c
    int func(void);
    ```
- Do not use ``clang-format`` or perform broad formatting edits unless instructed to do so.

### Naming

- Local variables: ``camelCase``
- Global variables: ``g_`` prefix
- File-scoped static variables: ``s_`` prefix where useful, although not essential
- Constants: ``UPPERCASE_SNAKE_CASE``
- Struct and enum typedefs: ``PascalCase`` with a descriptive ``snake_case`` suffix
- Function parameters: Leading underscore followed by normal variable convention
- Independent functions: ``parseCmd()``
- Functions owned by or dependent on another function or module: ``main_parseCmd()``
- Struct-associated functions: ``MyStruct_myAction()``
- Static helpers and utilities: descriptive module-prefixed names where appropriate
- Avoid vague names, such as ``data``, ``temp``, ``thing``, or ``value`` when a more precise name is available. Short conventional names, such as ``i``, may be used for small, obvious loop indices.

#### Examples

```c
typedef struct
{
    double x, y;
}
MyStruct;

double MyStruct_myAction(MyStruct *_self)
{
    return sqrt(_self->x * _self->x + _self->y * _self->y);
}
```

### Types and Qualifiers

- Use the narrowest appropriate integer type when the valid range is small and clearly bounded.
- Favor unsigned types for values that cannot be negative.
- Consider integer promotion, comparison rules, sentinel values, and wraparound behavior before using narrow unsigned types.
- Do not construct an unsigned descending-loop condition like ``i >= 0``, since it is always true.
- Opt for fixed-width integer types when the size of the representation matters.
- Choose ``bool`` for Boolean state or logic.
- Apply ``const``, ``restrict``, ``static``, and ``inline`` deliberately where their guarantees are valid.
- Do not add qualifiers merely for appearance; they must meet their semantic criteria.
- Steer clear of implicit narrowing conversions that may result in information loss.
- Employ explicit casts only when the conversion is purposeful and safe.

### Structs and Enums

- Structs and enums should typically be typedefed.
- Use the typedef name directly rather than repeating ``struct`` or ``enum`` at each declaration site.
- Keep struct-associated functions under the same naming prefix as the type.
- Preserve logical field ordering and consider alignment, padding, cache behavior, and total object size when modifying frequently allocated or hot structures.
- Do not reorder public or serialized structure fields without checking for compatibility issues.

### Functions

- Declare helper and utility functions ``static`` unless they are meant to be part of a wider interface.
- Define static helpers before the functions that use them wherever applicable.
- Ensure that functions accomplish one coherent task.
- Utilize module prefixes to clarify ownership and scope.
- Prefer explicit inputs and outputs over hidden global state.
- Validate pointer arguments before dereferencing them when ``NULL`` or ``nullptr`` is a potential failure mode.
- Do not split a small, cohesive function into many trivial wrappers solely to reduce line count.
- Do not make a function externally visible unless another translation unit genuinely needs it.

### Control Flow

- Always use braces with ``if``, ``else``, ``for``, ``while``, ``do``, and similar statements.
- Favor straightforward guard clauses over excessive nesting.
- Avoid unnecessary Boolean flags only to escape nested loops.
- Use ``goto`` only when it materially simplifies centralized cleanup or another well-defined local control-flow pattern.
- Do not introduce arbitrary or nonlocal jumps.
- Make ``switch`` cases clear and straightforward.
- Ensure that every intentional fallthrough is obvious, documented, and annotated.

### Memory Management

- Allocate using the pointed-to expression rather than repeating the type:
    ```c
    int *buffer = malloc(sizeof(*buffer) * N);
    ```
- Check the allocation results before use.
- Prevent overflow when multiplying by untrusted or potentially large values.
- Establish ownership clearly for every allocated resource.
- Free resources exactly once.
- Keep cleanup paths correct when adding new early returns.
- Prefer centralized cleanup when multiple resources have interdependent lifetimes.

### Arrays and Objects

- Choose compile-time array lengths over manually counting elements.
- Until ``_Countof`` is available and supported, use the established helper or:
    ```c
    sizeof(array) / sizeof(*array)
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

### Comments

- TBA

### Important Information for Automated Agents

- TBA