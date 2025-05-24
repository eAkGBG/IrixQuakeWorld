/*
 * IRIX compatibility stubs for OpenGL SGIS extensions
 * Copyright (C) 2025
 */

#include "quakedef.h"

// The glColorTableEXT function may not exist on IRIX GL
void glColorTableEXT(GLenum target, GLenum internalFormat, GLsizei width, 
                     GLenum format, GLenum type, const GLvoid *table)
{
    // This function doesn't exist on IRIX, so we provide an empty implementation
    Con_Printf("glColorTableEXT stub called (not supported on IRIX)\n");
}