/*
 * IRIX compatibility stubs for audio functions
 * Copyright (C) 2025
 */

#include "quakedef.h"

// Stub implementation for ALsetrate
long ALsetrate(long port, long rate)
{
    // This is a stub for IRIX audio library
    // In a full implementation, this would set the audio output rate
    Con_Printf("ALsetrate stub called: port=%ld, rate=%ld\n", port, rate);
    return 0;
}