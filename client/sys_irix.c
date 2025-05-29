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
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/syssgi.h> /* IRIX specific */

#include "quakedef.h"

int noconinput = 0;
int nostdout = 0;

static char basedir_buf[256] = ".";
char *basedir = basedir_buf;
char *cachedir = "/tmp";

cvar_t  sys_linerefresh = {"sys_linerefresh","0"};// set for entity display

// =======================================================================
// General routines
// =======================================================================

void Sys_DebugNumber(int y, int val)
{
}

void Sys_Printf (char *fmt, ...)
{
    va_list		argptr;
    char		text[2048]; 
    int chars_written;

    // if (nostdout) // Kommentera bort eller ta bort denna rad helt under testning med stderr
    //     return;

    va_start (argptr,fmt);
    chars_written = vsnprintf(text, sizeof(text), fmt, argptr);
    va_end (argptr);

    if (chars_written < 0) {
        // Försök skriva ett felmeddelande direkt till stderr om vsnprintf misslyckas
        // Inkludera originalformatsträngen för att identifiera vilken Sys_Printf som fallerade
        fprintf(stderr, "Sys_Printf: vsnprintf encoding error for format: %s\n", fmt);
        return;
    }
    // Om texten trunkerades kan du välja att indikera det, men det är valfritt.
    // if (chars_written >= (int)sizeof(text)) { // Jämför med int cast för att undvika varning
    //     fprintf(stderr, "Sys_Printf: output truncated for format: %s\n", fmt);
    // }

    // Ändra till stderr för testning
    fputs(text, stderr); 
    fflush(stderr);      
}

void Sys_Quit (void)
{
    Host_Shutdown();
    fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
    exit(0);
}

void Sys_Init(void)
{
#if id386
    // IRIX doesn't use x86 FPU code
    // Sys_SetFPCW();
#endif
}

void Sys_Error (char *error, ...)
{ 
    va_list     argptr;
    char        string[1024];

// change stdin to non blocking
    fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
    
    va_start (argptr,error);
    vsnprintf (string, sizeof(string), error,argptr); // Använd vsnprintf
    va_end (argptr);
    fprintf(stderr, "Error: %s\n", string);

    Host_Shutdown ();
    exit (1);
} 

void Sys_Warn (char *warning, ...)
{ 
    va_list     argptr;
    char        string[1024];
    
    va_start (argptr,warning);
    vsnprintf (string, sizeof(string), warning,argptr); // Använd vsnprintf
    va_end (argptr);
    fprintf(stderr, "Warning: %s", string); // Originalet saknade \n, behåll det om det är avsiktligt
} 

/*
============
Sys_FileTime

returns -1 if not present
============
*/
int	Sys_FileTime (char *path)
{
    struct	stat	buf;
    
    if (stat (path,&buf) == -1)
        return -1;
    
    return buf.st_mtime;
}


void Sys_mkdir (char *path)
{
    mkdir (path, 0777);
}

int Sys_FileOpenRead (char *path, int *handle)
{
    int	h;
    struct stat	fileinfo;
    
    
    h = open (path, O_RDONLY, 0666);
    *handle = h;
    if (h == -1)
        return -1;
    
    if (fstat (h,&fileinfo) == -1)
        Sys_Error ("Error fstating %s", path);

    return (int)fileinfo.st_size;
}

int Sys_FileOpenWrite (char *path)
{
    int     handle;

    umask (0);
    
    handle = open(path,O_RDWR | O_CREAT | O_TRUNC, 0666);

    if (handle == -1)
        Sys_Error ("Error opening %s: %s", path,strerror(errno));

    return handle;
}

int Sys_FileWrite (int handle, void *src, int count)
{
    return write (handle, src, count);
}

void Sys_FileClose (int handle)
{
    close (handle);
}

void Sys_FileSeek (int handle, int position)
{
    lseek (handle, position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
    return read (handle, dest, count);
}

void Sys_DebugLog(char *file, char *fmt, ...)
{
    va_list argptr; 
    static char data[1024]; // static buffer är ok här då den bara används av denna funktion
    int fd;
    
    va_start(argptr, fmt);
    vsnprintf(data, sizeof(data), fmt, argptr); // Använd vsnprintf
    va_end(argptr);
    fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd != -1) // Kontrollera om open lyckades
    {
        write(fd, data, strlen(data));
        close(fd);
    }
}

void Sys_EditFile(char *filename)
{
    char cmd[256];
    char *term;
    char *editor;

    term = getenv("TERM");
    if (term && !strcmp(term, "xterm"))
    {
        editor = getenv("VISUAL");
        if (!editor)
            editor = getenv("EDITOR");
        if (!editor)
            editor = getenv("EDIT");
        if (!editor)
            editor = "vi";
        sprintf(cmd, "xterm -e %s %s", editor, filename);
        system(cmd);
    }
}

double Sys_DoubleTime (void)
{
    struct timeval tp;
    struct timezone tzp; 
    static int      secbase; 
    
    gettimeofday(&tp, &tzp);  

    if (!secbase)
    {
        secbase = tp.tv_sec;
        return tp.tv_usec/1000000.0;
    }

    return (tp.tv_sec - secbase) + tp.tv_usec/1000000.0;
}

// =======================================================================
// Sleeps for microseconds
// =======================================================================

static volatile int oktogo;

void alarm_handler(int x)
{
    oktogo=1;
}

void Sys_LineRefresh(void)
{
}

void floating_point_exception_handler(int whatever)
{
//	Sys_Warn("floating point exception\n");
    signal(SIGFPE, floating_point_exception_handler);
}

char *Sys_ConsoleInput(void)
{
#if 0
    static char text[256];
    int     len;

    if (cls.state == ca_dedicated) {
        len = read (0, text, sizeof(text));
        if (len < 1)
            return NULL;
        text[len-1] = 0;    // rip off the /n and terminate

        return text;
    }
#endif
    return NULL;
}

#if !id386
// IRIX uses MIPS processors, not x86
void Sys_HighFPPrecision (void)
{
    // MIPS specific FP precision handling would go here if needed
}

void Sys_LowFPPrecision (void)
{
    // MIPS specific FP precision handling would go here if needed
}
#endif

int main (int c, char **v)
{
    double		time, oldtime, newtime;
    quakeparms_t parms;
    int j;

    // IRIX might need special FPU handling
    signal(SIGFPE, SIG_IGN);

    memset(&parms, 0, sizeof(parms));
    COM_InitArgv(c, v);

    parms.argc = com_argc;
    parms.argv = com_argv;

    parms.memsize = 16*1024*1024;

    j = COM_CheckParm("-mem");
    if (j)
        parms.memsize = (int) (Q_atof(com_argv[j+1]) * 1024 * 1024);
    
    parms.membase = malloc (parms.memsize);

    if (parms.membase == NULL) {
        fprintf(stderr, "FATAL ERROR: Failed to malloc %d bytes for membase at %s:%d.\n", parms.memsize, __FILE__, __LINE__);
        exit(1);
    }

    // Set up basedir
    strncpy(basedir_buf, ".", sizeof(basedir_buf)-1);
    basedir_buf[sizeof(basedir_buf)-1] = '\0';
    parms.basedir = basedir_buf;

    // Check for -noconinput parameter
    if (COM_CheckParm("-noconinput"))
    {
        noconinput = 1;
    }
    else
    {
        fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
    }

    // Check for -nostdout parameter
    if (COM_CheckParm("-nostdout"))
    {
        nostdout = 1;
    }
    Host_Init(&parms);

    oldtime = Sys_DoubleTime ();
    while (1)
    {
// find time spent rendering last frame
        newtime = Sys_DoubleTime ();
        time = newtime - oldtime;

        Host_Frame(time);
        oldtime = newtime;
    }
}

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
    int r;
    unsigned long addr;
    int psize = getpagesize();

    addr = (startaddr & ~(psize-1)) - psize;

    r = mprotect((char*)addr, length + startaddr - addr + psize, PROT_READ | PROT_WRITE | PROT_EXEC);        if (r < 0)
            Sys_Error("Protection change failed\n");
}