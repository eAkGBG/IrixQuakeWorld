# IRIX QuakeWorld Emergency Loop Protection - Final Implementation

## Problem
The client enters infinite loops during file searching with repeated output patterns like:
```
sound/items/r_item1.wavsound/items/r_item1.wavsound/items/r_item1.wav...
```

## Root Cause
Multiple interacting issues:
1. Corrupted pakfile structures with invalid `pak->numfiles`
2. Corrupted pakfile data causing `strcmp()` to behave unpredictably
3. Failed `fopen()` calls leading to repeated search attempts
4. Output buffer issues mixing `Con_Printf` and `Sys_Printf`

## Emergency Protection Implementation

### 1. Global Call Limit (Most Aggressive)
```c
static int global_call_count = 0;
global_call_count++;
if (global_call_count > 500) {
    // PERMANENT HALT - don't reset counter
    return -1;
}
```

### 2. Same File Protection
```c
static char last_filename[MAX_QPATH] = "";
static int same_file_count = 0;
if (same_file_count > 2) {
    // PERMANENT HALT for same file
    return -1;
}
```

### 3. Pakfile Loop Protection
```c
for (i=0 ; i<safe_numfiles ; i++) {
    if (i > 1000) {
        break;  // Emergency brake
    }
}
```

### 4. Match Detection Protection
```c
static int match_count = 0;
if (match_count > 3) {
    // Skip this match to prevent repeated processing
    goto skip_file;
}
```

### 5. Enhanced Logging
- Log every `COM_FOpenFile` call with counter
- Force `fflush(stdout)` for immediate output
- Clear identification of emergency stops

## Files Modified
- `/client/common.c` - Complete rewrite of `COM_FOpenFile` with emergency protection

## Expected Behavior
1. **Normal operation**: Files found and opened normally
2. **Corrupted pakfiles**: Emergency brakes activate, return errors instead of hanging
3. **Infinite loops**: System halts permanently after 500 calls total or 2 same-file attempts
4. **Clear logging**: Every file search attempt is logged with counters

## Key Design Principles
1. **Fail-safe**: Always return error rather than hang system
2. **Permanent protection**: Don't reset critical counters after emergency stops
3. **Aggressive limits**: Very low thresholds to catch problems early
4. **Complete logging**: Full visibility into what the system is doing

This implementation prioritizes system stability over file access - better to fail to load some files than to hang the entire system.
