# zx_ticks_get

## NAME

<!-- Updated by update-docs-from-abigen, do not edit. -->

Read the number of high-precision timer ticks since boot.

## SYNOPSIS

<!-- Updated by update-docs-from-abigen, do not edit. -->

```
#include <zircon/syscalls.h>

zx_ticks_t zx_ticks_get(void);
```

## DESCRIPTION

`zx_ticks_get()` returns the number of high-precision timer ticks since boot.

These ticks may be processor cycles, high speed timer, profiling timer, etc.
They are not guaranteed to continue advancing when the system is asleep.

## RIGHTS

<!-- Updated by update-docs-from-abigen, do not edit. -->

TODO(ZX-2399)

## RETURN VALUE

`zx_ticks_get()` returns the number of high-precision timer ticks since boot.

## ERRORS

`zx_ticks_get()` does not report any error conditions.

## NOTES

The returned value may be highly variable. Factors that can affect it include:
- Changes in processor frequency
- Migration between processors
- Reset of the processor cycle counter
- Reordering of instructions (if required, use a memory barrier)

## SEE ALSO


[ticks_per_second](ticks_per_second.md)
