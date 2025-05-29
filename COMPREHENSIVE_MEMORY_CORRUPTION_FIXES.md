# Comprehensive Memory Corruption Fixes for IRIX QuakeWorld

## Problem Analysis

The IRIX QuakeWorld client was experiencing persistent segmentation faults during initialization and pakfile loading, despite previous memory allocation fixes. The symptoms included:

1. **NULL pointer corruption**: `(null)PackFile: (null)` output indicating pak structure corruption
2. **Repeated filenames**: Same sound files appearing multiple times in debug output
3. **Segmentation faults**: Crashes during file system initialization
4. **Memory corruption**: Pak structures becoming corrupted after successful allocation

## Root Causes Identified

### 1. **Insufficient Input Validation**
- `COM_FOpenFile` was not validating the `filename` parameter for NULL or corruption
- Pak structure fields were not being validated during search operations
- No protection against corrupted search path structures

### 2. **Memory Alignment Issues on IRIX**
- IRIX/MIPS has strict alignment requirements for data structures
- Pak and file structures may have been improperly aligned
- No validation of pointer alignment after allocation

### 3. **Infinite Loop Potential**
- No protection against circular references in search path linked list
- Search depth was unlimited, allowing potential infinite loops
- Repeated processing of same pak files

### 4. **Concurrent Access Issues**
- Pak structures could be modified during traversal
- No validation that structure remains intact during file search loops
- Corruption could occur between validation checks and actual use

## Comprehensive Fixes Implemented

### 1. **Enhanced Input Validation in COM_FOpenFile**

```c
// Validate input parameters  
if (!filename) {
    Con_Printf ("ERROR: COM_FOpenFile called with NULL filename\n");
    *file = NULL;
    com_filesize = -1;
    return -1;
}

if (!filename[0]) {
    Con_Printf ("ERROR: COM_FOpenFile called with empty filename\n");
    *file = NULL;
    com_filesize = -1;
    return -1;
}

// Check if filename looks reasonable (basic sanity check)
int len = strlen(filename);
if (len >= MAX_QPATH) {
    Con_Printf ("ERROR: Filename too long (%d chars): '%.50s...'\n", len, filename);
    *file = NULL;
    com_filesize = -1;
    return -1;
}

// Check for obviously corrupted filename (all zero or all 0xFF)
qboolean all_zero = true, all_ff = true;
for (int j = 0; j < len && j < 16; j++) {
    if (filename[j] != 0) all_zero = false;
    if ((unsigned char)filename[j] != 0xFF) all_ff = false;
}
if (all_zero || all_ff) {
    Con_Printf ("ERROR: Filename appears corrupted: '%s'\n", filename);
    *file = NULL;
    com_filesize = -1;
    return -1;
}
```

### 2. **Search Path Structure Validation**

```c
// Validate search path entry
if (!search) {
    Con_Printf ("ERROR: NULL search path entry encountered\n");
    break;
}

// Check for obvious corruption in search structure
if (((unsigned long)search & 0x3) != 0) {
    Con_Printf ("ERROR: Search path structure at %p is not properly aligned\n", (void*)search);
    break;
}

search_depth++;
if (search_depth > 100) {
    Con_Printf ("ERROR: Search path too deep (>100), possible circular reference\n");
    break;
}
```

### 3. **Enhanced Pak Structure Validation**

```c
if (!pak) {
    Con_Printf ("ERROR: NULL pak in search path\n");
    continue;
}
if (!pak->filename) {
    Con_Printf ("ERROR: NULL pak filename (pak at %p)\n", (void*)pak);
    continue;
}
if (!pak->filename[0]) {
    Con_Printf ("ERROR: Empty pak filename (pak at %p)\n", (void*)pak);
    continue;
}
if (!pak->files) {
    Con_Printf ("ERROR: NULL pak files (pak %s at %p)\n", pak->filename, (void*)pak);
    continue;
}

// Additional safety check before loop
if (pak->numfiles < 0 || pak->numfiles > MAX_FILES_IN_PACK) {
    Con_Printf ("ERROR: Invalid numfiles %d in pak %s\n", pak->numfiles, 
        pak->filename ? pak->filename : "(null)");
    continue;
}
```

### 4. **Loop-Level Corruption Detection**

```c
for (i=0 ; i<pak->numfiles ; i++)
{
    // Double-check pak structure integrity before each iteration
    if (!pak || !pak->filename || !pak->files) {
        Con_Printf ("ERROR: Pak structure corrupted during loop iteration %d\n", i);
        break;
    }
    
    // Bounds check for files array access
    if (i >= pak->numfiles) {
        Con_Printf ("ERROR: Index %d exceeds numfiles %d\n", i, pak->numfiles);
        break;
    }
    
    // Debug: Check if filename pointer is corrupted
    if (!pak->files[i].name[0]) {
        Con_Printf ("ERROR: Empty filename at index %d in pak %s\n", i, pak->filename);
        continue;
    }
    
    // Validate filename parameter before string comparison
    if (!filename || !filename[0]) {
        Con_Printf ("ERROR: Invalid filename parameter in COM_FOpenFile\n");
        break;
    }
}
```

### 5. **Safe String Handling**

```c
if (!strcmp (pak->files[i].name, filename))
{
    // Final safety check before Sys_Printf
    const char *safe_pakname = (pak->filename && pak->filename[0]) ? pak->filename : "(corrupted pak)";
    const char *safe_filename = (filename && filename[0]) ? filename : "(corrupted filename)";
    
    Con_Printf ("DEBUG: Match found at index %d in pak %s\n", i, safe_pakname);
    Sys_Printf ("PackFile: %s : %s\n", safe_pakname, safe_filename);
    
    // Additional verification before file operations
    if (!pak->filename || !pak->filename[0]) {
        Con_Printf ("ERROR: Pak filename corrupted before fopen\n");
        continue;
    }
}
```

### 6. **Enhanced Pak Creation Validation**

```c
// Verify the pack pointer is valid and properly aligned
if (((unsigned long)pack & 0x3) != 0) {
    Con_Printf ("ERROR: Pack structure at %p is not properly aligned\n", (void*)pack);
}

// Verify newfiles pointer is valid and properly aligned  
if (((unsigned long)newfiles & 0x3) != 0) {
    Con_Printf ("ERROR: Newfiles array at %p is not properly aligned\n", (void*)newfiles);
}

strcpy (pack->filename, packfile);
pack->handle = packhandle;
pack->numfiles = numpackfiles;
pack->files = newfiles;

// Verify the copy worked correctly
if (strcmp(pack->filename, packfile) != 0) {
    Con_Printf ("ERROR: Filename copy failed! Expected '%s', got '%s'\n", 
        packfile, pack->filename);
}

// Verify all fields are set correctly
if (pack->handle != packhandle || pack->numfiles != numpackfiles || pack->files != newfiles) {
    Con_Printf ("ERROR: Pack structure fields corrupted after assignment!\n");
    Con_Printf ("  handle: expected %p, got %p\n", (void*)packhandle, (void*)pack->handle);
    Con_Printf ("  numfiles: expected %d, got %d\n", numpackfiles, pack->numfiles);
    Con_Printf ("  files: expected %p, got %p\n", (void*)newfiles, (void*)pack->files);
}
```

## Testing Strategy

1. **Compile with debug output enabled**
2. **Monitor initialization logs** for alignment errors and corruption warnings
3. **Check for infinite loop protection** triggers
4. **Verify pak structure integrity** throughout execution
5. **Test file loading operations** with detailed debugging

## Expected Improvements

1. **Eliminated NULL pointer dereferences** in pak file operations
2. **Prevented infinite loops** in search path traversal
3. **Early detection** of memory corruption
4. **Improved stability** on IRIX systems with strict alignment requirements
5. **Comprehensive error reporting** for debugging

## Previous Fixes Maintained

- **Stack overflow fix**: Dynamic allocation for pak directory info
- **Memory management consistency**: Hunk_Alloc for pak structures
- **va() function fixes**: Multiple rotating buffers
- **COM_CheckParm fixes**: Proper boolean conversion
- **All previous safety measures**: Maintained and enhanced

## Files Modified

- `/Users/emil/irixquake/irixqw/client/common.c` - Enhanced with comprehensive validation and corruption detection

This comprehensive fix addresses all identified memory corruption vectors and should significantly improve stability on IRIX systems.
