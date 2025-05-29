# CRITICAL: Buffer Overrun Fix for IRIX QuakeWorld

## Latest Critical Discovery: Non-Null-Terminated Pakfile Names

### The Smoking Gun
```
sound/items/r_item1.wavsound/items/r_item1.wavsound/items/r_item1.wav...
gfx/pop.lmpgfx/pop.lmpPackFile: (null) : ./id1/pak1.pak
```

**Problem**: Filnamn klistras ihop utan separatorer/null-terminatorer!

### Root Cause Analysis

**The `dpackfile_t` Structure Problem**:
```c
typedef struct
{
    char    name[56];      // EXACTLY 56 bytes - NO null terminator guaranteed!
    int     filepos, filelen;
} dpackfile_t;
```

**The Dangerous Code**:
```c
// BEFORE (DANGEROUS):
strncpy (newfiles[i].name, info[i].name, MAX_QPATH-1);  // info[i].name NOT null-terminated!
```

**What Happens**:
1. Pak files store filenames as **exactly 56 characters** 
2. If filename is 56 chars, **NO null terminator exists**
3. `strncpy()` reads `info[i].name` as a string, but **keeps reading past the array**
4. This causes **buffer overrun** reading into next structure fields
5. Results in **corrupted filenames** and **memory corruption cascade**

### The Fix Applied

```c
// FIXED: Safe handling of non-null-terminated pakfile names
for (i=0 ; i<numpackfiles ; i++)
{
    // CRITICAL FIX: info[i].name is NOT null-terminated if exactly 56 chars!
    // We must manually ensure null-termination before any string operations
    char safe_name[57];  // 56 + 1 for null terminator
    memcpy(safe_name, info[i].name, 56);
    safe_name[56] = 0;  // Force null termination
    
    // Now safely copy to newfiles with proper bounds checking
    strncpy (newfiles[i].name, safe_name, MAX_QPATH-1);
    newfiles[i].name[MAX_QPATH-1] = 0;  // Double ensure null termination
    
    newfiles[i].filepos = LittleLong(info[i].filepos);
    newfiles[i].filelen = LittleLong(info[i].filelen);
}
```

### Why This Causes the Exact Symptoms We Saw

1. **`strncpy(dest, info[i].name, len)`** tries to read a null-terminated string
2. **If `info[i].name` has no null terminator**, `strncpy` keeps reading memory
3. **It reads into next `dpackfile_t` entries** in the info array
4. **This creates concatenated filenames** like `sound/items/r_item1.wavsound/items/r_item1.wav`
5. **Memory corruption spreads** to pak structures and beyond
6. **Eventually causes segmentation fault**

### Complete Fix History

This fix builds on all previous memory corruption fixes:

1. **✅ va() function fix** - Rotating buffers (DONE)
2. **✅ COM_CheckParm fixes** - Proper boolean conversion (DONE) 
3. **✅ Stack overflow fix** - Dynamic allocation for pak directory (DONE)
4. **✅ Memory alignment fixes** - Added validation (DONE)
5. **✅ Comprehensive validation** - Added defensive checks (DONE)
6. **🆕 BUFFER OVERRUN FIX** - Safe pakfile name handling (NEW)

### Files Modified
- `/Users/emil/irixquake/irixqw/client/common.c` - Added safe pakfile name parsing

### Expected Result
After this fix, the repeated/concatenated filenames should stop appearing, and the segmentation fault should be eliminated.

### Testing on SGI
1. Copy updated `common.c` to SGI server
2. Compile with `make -f Makefile.Irix`  
3. Run and check for:
   - ✅ No more repeated filenames
   - ✅ Proper null-terminated strings in debug output
   - ✅ No segmentation fault
   - ✅ Successful pakfile loading

This was likely the **primary cause** of the persistent memory corruption on IRIX systems.
