# IRIX QuakeWorld - Syntax Error Fix

## Problem
Compilation error in `client/common.c` at line 1554:
```
client/common.c:1554: parse error before "else"
```

## Root Cause
Missing closing brace `}` for the inner `for` loop in `COM_FOpenFile()` function.

The function structure should be:
```c
for (search = com_searchpaths ; search ; search = search->next)
{
    if (search->pack)
    {
        for (i=0 ; i<pak->numfiles ; i++)  // Inner loop
        {
            // ... check pak files ...
        }  // <-- This closing brace was missing!
    }
    else
    {
        // ... directory check ...
    }
}
```

## Fix Applied
Added the missing closing brace `}` after the pak file iteration loop and before the `else` clause.

**File:** `/Users/emil/irixquake/irixqw/client/common.c`
**Line:** ~1553 (after the pak file name comparison block)

## Status
✅ **FIXED** - Syntax error resolved, code should now compile properly.

This was a simple syntax error that occurred during the previous memory corruption fixes. The logical structure and functionality remain unchanged.
