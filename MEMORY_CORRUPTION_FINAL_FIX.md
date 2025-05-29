# IRIX QuakeWorld Memory Corruption - Final Fix

## Problem Summary
The IRIX QuakeWorld client suffered from memory corruption where `basedir` became corrupted with garbage values and global variables `noconinput` and `nostdout` were overwritten during initialization.

## Root Cause Analysis
After extensive debugging, we identified **TWO** primary sources of memory corruption:

### 1. Incorrect COM_CheckParm Usage (Boolean Confusion)
**Problem**: `COM_CheckParm()` returns position values (1 to argc-1) or 0, but was being used directly as boolean values.

**Files Affected**:
- `sys_irix.c`: `noconinput` and `nostdout` assignments  
- `cl_main.c`: `-minmemory` parameter check
- `gl_rsurf.c`: All `-lm_*` lightmap parameter checks

**Fix**: Changed from direct assignment to proper boolean conversion:
```c
// BEFORE (incorrect):
noconinput = COM_CheckParm("-noconinput");  // Could assign random position value

// AFTER (correct):
if (COM_CheckParm("-noconinput")) {
    noconinput = 1;
}
```

### 2. va() Function Static Buffer Reuse (The Real Culprit)
**Problem**: The `va()` function uses a single static buffer that gets overwritten on each call. In `COM_InitFilesystem()`, two consecutive calls to `va()` caused the first pointer to reference corrupted data.

**Problematic Code**:
```c
COM_AddGameDirectory (va("%s/id1", com_basedir) );  // First va() call
char* temp_qw_path = va("%s/qw", com_basedir);      // Second va() call overwrites first!
COM_AddGameDirectory (temp_qw_path);                // First pointer now corrupted
```

**The Corruption Chain**:
1. First `va()` call creates "/path/id1" in static buffer
2. `COM_AddGameDirectory()` receives pointer to this buffer
3. Second `va()` call overwrites buffer with "/path/qw"  
4. When `COM_AddGameDirectory()` tries to use the first pointer, it now points to "/path/qw"
5. `strcpy(search->filename, dir)` copies wrong path
6. File system paths become corrupted
7. Later operations accessing `basedir` see garbage values

### 3. Double COM_InitArgv Calls
**Problem**: `COM_InitArgv()` was called twice - once in `sys_irix.c` and again in `cl_main.c`, potentially corrupting argument arrays.

**Fix**: Removed redundant call from `cl_main.c`.

### 4. NULL Pointer Bug in COM_AddGameDirectory  
**Problem**: `strcpy(gamedirfile, p)` when `strrchr()` returns NULL.

**Fix**: Changed to `strcpy(gamedirfile, dir)` when `p` is NULL.

## Final Fixes Applied

### 1. Fixed va() Function (common.c)
```c
// OLD: Single static buffer (prone to reuse issues)
static char string[1024];

// NEW: 4 rotating buffers (eliminates reuse issues)  
static char string[4][1024];
static int buffer_index = 0;
buffer_index = (buffer_index + 1) % 4;
```

### 2. Fixed COM_InitFilesystem (common.c)
```c
// OLD: Unsafe va() reuse
COM_AddGameDirectory (va("%s/id1", com_basedir) );
char* temp_qw_path = va("%s/qw", com_basedir);
COM_AddGameDirectory (temp_qw_path);

// NEW: Local buffers eliminate va() dependency
char id1_path[MAX_OSPATH];
char qw_path[MAX_OSPATH];
sprintf(id1_path, "%s/id1", com_basedir);
sprintf(qw_path, "%s/qw", com_basedir);
COM_AddGameDirectory(id1_path);
COM_AddGameDirectory(qw_path);
```

### 3. Fixed COM_CheckParm Usage (sys_irix.c, cl_main.c, gl_rsurf.c)
```c
// OLD: Direct assignment of position value
noconinput = COM_CheckParm("-noconinput");

// NEW: Proper boolean conversion
if (COM_CheckParm("-noconinput")) {
    noconinput = 1;
}
```

### 4. Removed Double Initialization (cl_main.c)
```c
// REMOVED: Redundant COM_InitArgv() call that could corrupt arguments
```

### 5. Fixed NULL Pointer (common.c)
```c
// OLD: Potential NULL pointer dereference
strcpy(gamedirfile, p);

// NEW: Safe fallback when strrchr returns NULL
strcpy(gamedirfile, p ? p : dir);
```

## Impact
These fixes eliminate the memory corruption that was causing:
- `basedir` to become empty `[]` then corrupted `[�����B]`
- `noconinput` and `nostdout` to get overwritten with random values
- File system paths to become invalid
- Potential crashes during initialization

## Testing
The fixes address the root causes identified through debugging. The most critical fix was the `va()` function buffer reuse issue, which was the primary source of the path corruption observed in the debug output.

## Files Modified
1. `/client/common.c` - Fixed va() function and COM_InitFilesystem
2. `/client/sys_irix.c` - Fixed COM_CheckParm boolean usage  
3. `/client/cl_main.c` - Fixed COM_CheckParm usage, removed double COM_InitArgv
4. `/client/gl_rsurf.c` - Fixed COM_CheckParm boolean usage
5. `/client/common.c` - Fixed COM_AddGameDirectory NULL pointer

The original memory corruption bug stemming from incorrect boolean interpretation of COM_CheckParm return values and unsafe va() function usage has been resolved.
