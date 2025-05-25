/* /*
 * IRIX compatibility functions for Quake
 */
/*
#ifndef IRIX_COMPAT_H
#define IRIX_COMPAT_H

// String comparison (case insensitive) - Windows compatibility function
int stricmp(const char *s1, const char *s2)
{
    return strcasecmp(s1, s2);
}

// SGIS multitexture extension stub
void qglSelectTextureSGIS(GLenum texture)
{
    // No multitexture support on IRIX, this is just a stub
}

// NetGraph texture - this is just a stub to make linking work
int netgraphtexture;

// NetGraph render function stub
void R_NetGraph(void)
{
    // Stub implementation
}

#endif // IRIX_COMPAT_H */