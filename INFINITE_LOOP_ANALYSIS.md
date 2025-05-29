# IRIX QuakeWorld Infinite Loop Analysis

## Problem
The client is stuck in an infinite loop during pakfile searching, showing repeated output:
```
sound/items/r_item1.wavsound/items/r_item1.wavsound/items/r_item1.wav...
```

## Root Cause Analysis

### Pattern Analysis
1. **Repeated filenames without separators** - indicates printf/output buffer issues
2. **Same file repeated** - suggests infinite loop in file search
3. **No progression** - loop counter `i` may not be incrementing
4. **Final segfault** - eventually runs out of memory or hits corruption

### Potential Causes

#### 1. Corrupted pak->numfiles
- If `pak->numfiles` is corrupted to a huge value (like 0x7fffffff)
- Loop `for (i=0; i<pak->numfiles; i++)` would run almost forever
- Each iteration prints the same filename if `pak->files` points to same memory

#### 2. Corrupted pak->files pointer
- If `pak->files` points to invalid memory or repeating structure
- `pak->files[i].name` would always return the same string
- Loop would continue infinitely with same comparison

#### 3. String comparison issues
- If `pak->files[i].name` contains null bytes or corruption
- `strcmp()` might always return 0 (match)
- File would be "found" repeatedly but fopen() would fail

#### 4. Output buffer corruption
- `Con_Printf` or `Sys_Printf` might have buffer issues
- Same string gets printed repeatedly due to buffer reuse
- Creates appearance of infinite loop even if loop is finite

## Current Fixes Applied
1. Added loop counter limit (10,000 iterations)
2. Added emergency brake for file index
3. Replaced `Sys_Error()` with safe return on fopen failure
4. Added pak structure validation
5. Added filename corruption detection

## Next Steps
1. Examine pakfile loading in `COM_LoadPackFile`
2. Check if pak structures are corrupted during allocation
3. Verify `pak->numfiles` is reasonable value
4. Add more detailed debugging to track loop progress
5. Consider complete rewrite of file search with safer bounds checking
