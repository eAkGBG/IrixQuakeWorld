/*
Copyright (C) 1996-1997 Id Software, Inc.
IRIX port Copyright (C) 2025

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "quakedef.h"
#include "glquake.h"

// Global variables for video
viddef_t    vid;                // global video state
int    VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes = 0;

// 8-bit texture support check - IRIX doesn't properly support this extension
qboolean VID_Is8bit(void)
{
    return false; // IRIX doesn't support 8-bit textures in the way Quake expects
}

void VID_Init(unsigned char *palette)
{
    // This will be filled in when implementing the actual IRIX video code
}

void VID_Shutdown(void)
{
    // This will be filled in when implementing the actual IRIX video code
}