/*
 * IRIX compatibility stubs for CD audio functions
 * Copyright (C) 2025
 */

#include "quakedef.h"

// Note: These are just stubs to make the program link
// For actual CD functionality on IRIX, you'd need to implement these using the 
// appropriate IRIX libraries (like dmedia)

// Define our types to match dmedia/cdaudio.h
typedef void* CDPLAYER;

typedef struct {
    int state;
} CDSTATUS;

typedef struct {
    int dummy;
} CDTRACKINFO;

// CD Player functions
void* CDopenplayer(const char* device)
{
    Con_Printf("CD: CDopenplayer stub called for device: %s\n", device);
    return NULL;
}

int CDcloseplayer(void* player)
{
    Con_Printf("CD: CDcloseplayer stub called\n");
    return 0;
}

int CDeject(void* player)
{
    Con_Printf("CD: CDeject stub called\n");
    return 0;
}

int CDgetstatus(void* player, CDSTATUS* status)
{
    Con_Printf("CD: CDgetstatus stub called\n");
    // Set status to stopped
    if (status)
        status->state = 4; // CD_STOPPED
    return 0;
}

int CDtoggledoor(void* player)
{
    Con_Printf("CD: CDtoggledoor stub called\n");
    return 0;
}

int CDgettrackinfo(void* player, int track, CDTRACKINFO* info)
{
    Con_Printf("CD: CDgettrackinfo stub called for track %d\n", track);
    return -1;  // Indicate failure (no tracks available in stub)
}

int CDplaytrack(void* player, int track, int mode)
{
    Con_Printf("CD: CDplaytrack stub called for track %d, mode %d\n", track, mode);
    return 0;
}

int CDstop(void* player)
{
    Con_Printf("CD: CDstop stub called\n");
    return 0;
}

int CDpause(void* player)
{
    Con_Printf("CD: CDpause stub called\n");
    return 0;
}

int CDresume(void* player)
{
    Con_Printf("CD: CDresume stub called\n");
    return 0;
}