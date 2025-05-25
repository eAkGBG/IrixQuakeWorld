/*
 * IRIX compatibility functions for Quake
 * Copyright (C) 2025
 */

#include "quakedef.h"

// String comparison (case insensitive) - Windows compatibility function
/* int stricmp(const char *s1, const char *s2)
{
    return strcasecmp(s1, s2);
} */

// For netgraph texture
//int netgraphtexture;

// Network statistics display
/* void R_NetGraph(void)
{
    // Empty stub - network statistics display not implemented on IRIX
    Con_DPrintf("R_NetGraph stub called\n");
} */

// SGIS multitexture extension stub
/* void qglSelectTextureSGIS(GLenum texture)
{
    // No multitexture support on IRIX, this is just a stub
    Con_DPrintf("qglSelectTextureSGIS stub called: %d\n", texture);
} */