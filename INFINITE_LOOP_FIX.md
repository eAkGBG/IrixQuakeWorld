# IRIX QuakeWorld Infinite Loop Fix

## Problem Description
The IRIX QuakeWorld client gets stuck in an infinite loop during pakfile searching, producing repeated output like:
```
sound/items/r_item1.wavsound/items/r_item1.wavsound/items/r_item1.wav...
```

This eventually leads to a segmentation fault.

## Comprehensive Fixes Applied

### 1. Multiple Loop Protection Mechanisms

#### Global Loop Counter
- Added static `loop_count` that increments on each `COM_FOpenFile` call
- If count exceeds 10,000, function returns error and resets
- Prevents infinite recursive calls

#### Same File Detection
- Tracks `last_filename` and counts repeated searches for same file
- If same file searched more than 100 times, returns error
- Prevents getting stuck on single problematic file

#### Emergency File Index Brake
- Added check `if (i > 10000)` in pak file loop
- Breaks out of loop if file index gets unreasonably high
- Prevents infinite iteration through corrupted pak structures

### 2. Pak Structure Validation

#### Safe File Count Limiting
```c
int safe_numfiles = pak->numfiles;
if (safe_numfiles > MAX_FILES_IN_PACK || safe_numfiles < 0) {
    safe_numfiles = MAX_FILES_IN_PACK;
}
for (i=0 ; i<safe_numfiles ; i++)
```

#### Pointer Validation
- Check if `pak->files` pointer is in reasonable memory range
- Validate pointer is not NULL or obviously corrupted
- Skip pak if structure appears invalid

#### File Entry Validation
- Check for empty filenames (`!pak->files[i].name[0]`)
- Scan for non-printable characters in filenames
- Skip files with corrupted name data

### 3. Safe Error Handling

#### Replace Sys_Error with Safe Return
```c
// OLD: Sys_Error ("Couldn't reopen %s", pak->filename);
// NEW: 
if (!*file) {
    Con_Printf ("ERROR: Could not reopen pak file '%s'\n", pak->filename);
    loop_count = 0;
    com_filesize = -1;
    return -1;
}
```

#### Reset Counters on Success/Error
- Always reset `loop_count` when function completes successfully
- Reset counters when returning error to allow future attempts
- Prevents permanent lockout after error

### 4. Enhanced Debugging

#### Search Path Tracking
- Added circular search path detection (max 100 search paths)
- Log each search path with pak/filename info
- Track search progression to identify loops

#### Detailed File Comparison Logging
- Special logging for problematic files (r_item1.wav, basekey.wav)
- Show exact string comparisons being performed
- Track which files are being matched repeatedly

#### Memory and Structure Validation
- Log pak structure addresses and values
- Validate memory alignment of structures
- Check for obviously corrupted pointers

## Files Modified
- `/client/common.c` - COM_FOpenFile function with comprehensive safety measures

## Expected Behavior
With these fixes, the client should:
1. Never get stuck in infinite loops during file searching
2. Gracefully handle corrupted pak files
3. Provide detailed error messages for debugging
4. Continue operation even if some pak files are corrupted
5. Return appropriate error codes when files cannot be found

## Testing Notes
The fixes are designed to be fail-safe - if anything goes wrong, the function will return an error rather than hanging the system. This allows the game to continue running even with corrupted pak files, though some files may not be accessible.
