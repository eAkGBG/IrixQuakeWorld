/*
 * IRIX compatibility functions for Quake
 */
/* 
#ifndef IRIX_COMPAT_H
#define IRIX_COMPAT_H

#include <strings.h>
#include <GL/gl.h>

// String comparison (case insensitive) - Windows compatibility function
int stricmp(const char *s1, const char *s2);

// SGIS multitexture extension stub
void qglSelectTextureSGIS(GLenum texture);

// NetGraph texture - this is just a stub to make linking work
extern int netgraphtexture;

// NetGraph render function stub
void R_NetGraph(void);

#endif // IRIX_COMPAT_H */