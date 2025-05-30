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
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
// #include <sys/vt.h> // Inte tillgänglig på IRIX
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
// #include <asm/io.h> // Inte tillgänglig på IRIX

// SGI-specifika inkluderingar
#include <sys/syssgi.h>
#include <sys/prctl.h>

// X11 inkluderingar
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

// OpenGL inkluderingar - SGI-specifika
#include <GL/gl.h>
#include <GL/glx.h> // Viktig för glXGetProcAddress
#include "quakedef.h"

#define WARP_WIDTH              320
#define WARP_HEIGHT             200

static Display *dpy = NULL;
static Window win;
static GLXContext ctx = NULL;

unsigned short d_8to16table[256];
unsigned int d_8to24table[256];
unsigned char d_15to8table[65536];

static qboolean usedga = false;

#define stringify(m) { #m, m }

cvar_t vid_mode = {"vid_mode","0",false};
 
cvar_t  mouse_button_commands[3] =
{
    {"mouse1","+attack"},
    {"mouse2","+strafe"},
    {"mouse3","+forward"},
};

static int mouse_buttons=3;
static int mouse_buttonstate;
static int mouse_oldbuttonstate;
static float mouse_x, mouse_y;
static float p_mouse_x, p_mouse_y;
static float old_mouse_x, old_mouse_y;

cvar_t _windowed_mouse = {"_windowed_mouse", "1", true};
cvar_t m_filter = {"m_filter","0"};
static float old_windowed_mouse;

static int scr_width, scr_height;

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | \
            PointerMotionMask | ButtonMotionMask)

/*-----------------------------------------------------------------------*/

int		texture_mode = GL_LINEAR;
int		texture_extension_number = 1;

float		gldepthmin, gldepthmax;

cvar_t	gl_ztrick = {"gl_ztrick","1"};

// Dessa är korrekta definitioner för gl_sgis.c
qboolean is8bit = false;
qboolean isPermedia = false; // Om denna används specifikt i gl_sgis.c
qboolean gl_mtexable = false;

// Definitioner för multitexture-funktionspekarna (korrekt här)
lpMTexCoord2fSGISFUNC qglMTexCoord2fSGIS = NULL;
lpSelTexSGISFUNC qglSelectTextureSGIS = NULL;

/*-----------------------------------------------------------------------*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

/*
=================
VID_Gamma_f

Keybinding command
=================
*/
void VID_Gamma_f (void)
{
    float	gamma, f, inf;
    unsigned char	palette[768];
    int		i;

    if (Cmd_Argc () == 2)
    {
        gamma = Q_atof (Cmd_Argv(1));

        for (i=0 ; i<768 ; i++)
        {
            f = pow ( (host_basepal[i]+1)/256.0 , gamma );
            inf = f*255 + 0.5;
            if (inf < 0)
                inf = 0;
            if (inf > 255)
                inf = 255;
            palette[i] = inf;
        }

        VID_SetPalette (palette);

        vid.recalc_refdef = 1;				// force a surface cache flush
    }
}

void VID_Shutdown(void)
{
    if (!ctx)
        return;

    XUngrabPointer(dpy,CurrentTime);
    XUngrabKeyboard(dpy,CurrentTime);

    glXDestroyContext(dpy,ctx);
}

int XLateKey(XKeyEvent *ev)
{

    int key;
    char buf[64];
    KeySym keysym;

    key = 0;

    XLookupString(ev, buf, sizeof buf, &keysym, 0);

    switch(keysym)
    {
        case XK_KP_Page_Up:
        case XK_Page_Up:	 key = K_PGUP; break;

        case XK_KP_Page_Down:
        case XK_Page_Down:	 key = K_PGDN; break;

        case XK_KP_Home:
        case XK_Home:	 key = K_HOME; break;

        case XK_KP_End:
        case XK_End:	 key = K_END; break;

        case XK_KP_Left:
        case XK_Left:	 key = K_LEFTARROW; break;

        case XK_KP_Right:
        case XK_Right:	key = K_RIGHTARROW;		break;

        case XK_KP_Down:
        case XK_Down:	 key = K_DOWNARROW; break;

        case XK_KP_Up:
        case XK_Up:		 key = K_UPARROW;	 break;

        case XK_Escape: key = K_ESCAPE;		break;

        case XK_KP_Enter:
        case XK_Return: key = K_ENTER;		 break;

        case XK_Tab:		key = K_TAB;			 break;

        case XK_F1:		 key = K_F1;				break;

        case XK_F2:		 key = K_F2;				break;

        case XK_F3:		 key = K_F3;				break;

        case XK_F4:		 key = K_F4;				break;

        case XK_F5:		 key = K_F5;				break;

        case XK_F6:		 key = K_F6;				break;

        case XK_F7:		 key = K_F7;				break;

        case XK_F8:		 key = K_F8;				break;

        case XK_F9:		 key = K_F9;				break;

        case XK_F10:		key = K_F10;			 break;

        case XK_F11:		key = K_F11;			 break;

        case XK_F12:		key = K_F12;			 break;

        case XK_BackSpace: key = K_BACKSPACE; break;

        case XK_KP_Delete:
        case XK_Delete: key = K_DEL; break;

        case XK_Pause:	key = K_PAUSE;		 break;

        case XK_Shift_L:
        case XK_Shift_R:	key = K_SHIFT;		break;

        case XK_Execute: 
        case XK_Control_L: 
        case XK_Control_R:	key = K_CTRL;		 break;

        case XK_Alt_L:	
        case XK_Meta_L: 
        case XK_Alt_R:	
        case XK_Meta_R: key = K_ALT;			break;

        case XK_KP_Begin: key = K_AUX30;	break;

        case XK_Insert:
        case XK_KP_Insert: key = K_INS; break;

        case XK_KP_Multiply: key = '*'; break;
        case XK_KP_Add: key = '+'; break;
        case XK_KP_Subtract: key = '-'; break;
        case XK_KP_Divide: key = '/'; break;

        default:
            key = *(unsigned char*)buf;
            if (key >= 'A' && key <= 'Z')
                key = key - 'A' + 'a';
            break;
    } 

    return key;
}

struct
{
    int key;
    int down;
} keyq[64];
int keyq_head=0;
int keyq_tail=0;

int config_notify=0;
int config_notify_width;
int config_notify_height;
                              
qboolean Keyboard_Update(void)
{
    XEvent x_event;
   
    if(!XCheckMaskEvent(dpy,KEY_MASK,&x_event))
        return false;

    switch(x_event.type) {
    case KeyPress:
        keyq[keyq_head].key = XLateKey(&x_event.xkey);
        keyq[keyq_head].down = true;
        keyq_head = (keyq_head + 1) & 63;
        break;
    case KeyRelease:
        keyq[keyq_head].key = XLateKey(&x_event.xkey);
        keyq[keyq_head].down = false;
        keyq_head = (keyq_head + 1) & 63;
        break;
    }

    return true;
}

qboolean Mouse_Update(void)
{
    XEvent x_event;
    int b;
   
    if(!XCheckMaskEvent(dpy,MOUSE_MASK,&x_event))
        return false;

    switch(x_event.type) {
    case MotionNotify:
        if (usedga) {
            mouse_x += x_event.xmotion.x_root;
            mouse_y += x_event.xmotion.y_root;
        } else if (_windowed_mouse.value) {
            mouse_x += (float) ((int)x_event.xmotion.x - (int)(scr_width/2));
            mouse_y += (float) ((int)x_event.xmotion.y - (int)(scr_height/2));

            /* move the mouse to the window center again */
            XSelectInput(dpy,win, (KEY_MASK | MOUSE_MASK) & ~PointerMotionMask);
            XWarpPointer(dpy,None,win,0,0,0,0, (scr_width/2),(scr_height/2));
            XSelectInput(dpy,win, KEY_MASK | MOUSE_MASK);
        } else {
            mouse_x = (float) (x_event.xmotion.x-p_mouse_x);
            mouse_y = (float) (x_event.xmotion.y-p_mouse_y);
            p_mouse_x=x_event.xmotion.x;
            p_mouse_y=x_event.xmotion.y;
        }
        break;

    case ButtonPress:
        b=-1;
        if (x_event.xbutton.button == 1)
            b = 0;
        else if (x_event.xbutton.button == 2)
            b = 2;
        else if (x_event.xbutton.button == 3)
            b = 1;
        if (b>=0)
            mouse_buttonstate |= 1<<b;
        break;

    case ButtonRelease:
        b=-1;
        if (x_event.xbutton.button == 1)
            b = 0;
        else if (x_event.xbutton.button == 2)
            b = 2;
        else if (x_event.xbutton.button == 3)
            b = 1;
        if (b>=0)
            mouse_buttonstate &= ~(1<<b);
        break;
    }
   
    if (old_windowed_mouse != _windowed_mouse.value) {
        old_windowed_mouse = _windowed_mouse.value;

        if (!_windowed_mouse.value) {
            /* ungrab the pointer */
            Con_Printf("Releasing mouse.\n");

            XUngrabPointer(dpy,CurrentTime);
            XUngrabKeyboard(dpy,CurrentTime);
        } else {
            /* grab the pointer */
            Con_Printf("Grabbing mouse.\n");

            XGrabPointer(dpy,win,False,MOUSE_MASK,GrabModeAsync,
                GrabModeAsync,win,None,CurrentTime);
            XWarpPointer(dpy,None,win, 0,0,0,0, scr_width/2, scr_height/2);
            XGrabKeyboard(dpy,win,
                False,
                GrabModeAsync,GrabModeAsync,
                CurrentTime);
        }
    }
    return true;
}

void signal_handler(int sig)
{
    printf("Received signal %d, exiting...\n", sig);
    VID_Shutdown();
    exit(0);
}

void InitSig(void)
{
    signal(SIGHUP, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGILL, signal_handler);
    signal(SIGTRAP, signal_handler);
    signal(SIGIOT, signal_handler);
    signal(SIGBUS, signal_handler);
    signal(SIGFPE, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGTERM, signal_handler);
}

void VID_ShiftPalette(unsigned char *p)
{
    VID_SetPalette(p);
}

void	VID_SetPalette (unsigned char *palette)
{
    byte	*pal;
    unsigned short r,g,b;
    int     v;
    int     r1,g1,b1;
    int		k;
    unsigned short i;
    unsigned	*table;
    FILE *f;
    char s[255];
    float dist, bestdist;
    static qboolean palflag = false;

//
// 8 8 8 encoding
//
    pal = palette;
    table = d_8to24table;
    for (i=0 ; i<256 ; i++)
    {
        r = pal[0];
        g = pal[1];
        b = pal[2];
        pal += 3;
        
        // SGI OpenGL förväntar sig RGB-ordning, inte BGR
        v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
        *table++ = v;
    }
    d_8to24table[255] &= 0xffffff;	// 255 is transparent

    // JACK: 3D distance calcs - k is last closest, l is the distance.
    // FIXME: Precalculate this and cache to disk.
    if (palflag)
        return;
    palflag = true;

    COM_FOpenFile("glquake/15to8.pal", &f);
    if (f) {
        fread(d_15to8table, 1<<15, 1, f);
        fclose(f);
    } else {
        for (i=0; i < (1<<15); i++) {
            /* Maps
             000000000000000
             000000000011111 = Red  = 0x1F
             000001111100000 = Blue = 0x03E0
             111110000000000 = Grn  = 0x7C00
             */
             r = ((i & 0x1F) << 3)+4;
             g = ((i & 0x03E0) >> 2)+4;
             b = ((i & 0x7C00) >> 7)+4;
            pal = (unsigned char *)d_8to24table;
            for (v=0,k=0,bestdist=10000.0; v<256; v++,pal+=4) {
                 r1 = (int)r - (int)pal[0];
                 g1 = (int)g - (int)pal[1];
                 b1 = (int)b - (int)pal[2];
                dist = sqrt(((r1*r1)+(g1*g1)+(b1*b1)));
                if (dist < bestdist) {
                    k=v;
                    bestdist = dist;
                }
            }
            d_15to8table[i]=k;
        }
        sprintf(s, "%s/glquake", com_gamedir);
         Sys_mkdir (s);
        sprintf(s, "%s/glquake/15to8.pal", com_gamedir);
        if ((f = fopen(s, "wb")) != NULL) {
            fwrite(d_15to8table, 1<<15, 1, f);
            fclose(f);
        }
    }
}

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
    // These will use the global variables declared in glquake.h and defined in gl_rmain.c
    // They are initialized to NULL in gl_rmain.c and will be populated here.
    if (!gl_vendor) gl_vendor = (const char *)glGetString(GL_VENDOR);
    if (!gl_renderer) gl_renderer = (const char *)glGetString(GL_RENDERER);
    if (!gl_version) gl_version = (const char *)glGetString(GL_VERSION);
    if (!gl_extensions) gl_extensions = (const char *)glGetString(GL_EXTENSIONS);

    Con_Printf ("GL_VENDOR: %s\n", gl_vendor ? gl_vendor : "Unknown");
    Con_Printf ("GL_RENDERER: %s\n", gl_renderer ? gl_renderer : "Unknown");
    Con_Printf ("GL_VERSION: %s\n", gl_version ? gl_version : "Unknown");
    Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions ? gl_extensions : "None");

    if (gl_vendor && strstr(gl_vendor, "SGI") != NULL) {
        Con_Printf("Using SGI-specific optimizations\n");
    }

    // Initiera multitexture
    gl_mtexable = false; 
    qglSelectTextureSGIS = NULL;
    qglMTexCoord2fSGIS = NULL;

    if (gl_extensions && strstr(gl_extensions, "GL_SGIS_multitexture"))
    {
        Con_Printf("GL_SGIS_multitexture found in extension string.\n");

        // On IRIX, SGI extensions are often directly linkable.
        // glXGetProcAddress is not linking, so we attempt direct assignment.
        // The function prototypes for glSelectTextureSGIS and glMultiTexCoord2fSGIS
        // have been added to glquake.h.

        qglSelectTextureSGIS = glSelectTextureSGIS;
        // qglMTexCoord2fSGIS = glMTexCoord2fSGIS; // Original
        qglMTexCoord2fSGIS = glMultiTexCoord2fSGIS; // Prova denna istället

        // If the functions linked successfully, the pointers will be non-NULL.
        // The linker would have failed if the symbols weren't found.
        if (qglSelectTextureSGIS && qglMTexCoord2fSGIS) 
        {
            gl_mtexable = true;
            Con_Printf("SGIS multitexture functions assigned via direct linking.\n");
        }
        else
        {
            // This case is unlikely if direct linking is the method, as unresolved symbols
            // would typically cause a linker error.
            Con_Printf("ERROR: GL_SGIS_multitexture reported, but direct assignment of function pointers failed (resulted in NULL).\n");
            Con_Printf("Check linker output for unresolved symbols: glSelectTextureSGIS, glMultiTexCoord2fSGIS.\n");
            if (!qglSelectTextureSGIS) Con_Printf(" - glSelectTextureSGIS is NULL after assignment.\n");
            if (!qglMTexCoord2fSGIS) Con_Printf(" - qglMTexCoord2fSGIS (tried as glMultiTexCoord2fSGIS) is NULL after assignment.\n");
            qglSelectTextureSGIS = NULL; // Ensure they are NULL
            qglMTexCoord2fSGIS = NULL;
            gl_mtexable = false;
        }
    }
    else
    {
        Con_Printf("GL_SGIS_multitexture not found in extension string. Multitexture disabled.\n");
    }

    glClearColor (1,0,0,0);
    glCullFace(GL_FRONT);
    glEnable(GL_TEXTURE_2D);

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.666);

    glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
    glShadeModel (GL_FLAT);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
    extern cvar_t gl_clear;

    *x = *y = 0;
    *width = scr_width;
    *height = scr_height;
}


void GL_EndRendering (void)
{
    glFlush();
    glXSwapBuffers(dpy,win);
}

qboolean VID_Is8bit(void)
{
    return is8bit;
}

// SGI-specifik 8-bitarsfunktion
void VID_Init8bitPalette(void) 
{
    // char thePalette[256*3]; // Behövs inte om vi inte förbereder data för glColorTableEXT
    // int i; // Behövs inte heller i så fall

    // Sätt is8bit till false som standard.
    // Om GL_EXT_shared_texture_palette finns men vi inte kan/vill använda glColorTableEXT på IRIX,
    // så förblir is8bit false.
    is8bit = false;

    if (gl_extensions && strstr(gl_extensions, "GL_EXT_shared_texture_palette") != NULL) {
        Con_SafePrintf("8-bit GL extensions enabled (GL_EXT_shared_texture_palette found).\n");

        // Eftersom vi är i gl_sgis.c (IRIX-specifik kod) och vi har konstaterat
        // att glColorTableEXT antingen inte är tillgänglig via länkning mot systemets GL
        // eller inte är önskvärd att använda, så anropar vi den inte här.
        // Konsekvensen är att det specifika 8-bitarsläget som denna extension
        // möjliggör inte kommer att användas.
        // Con_Printf(PRINT_DEVELOPER, "IRIX: GL_EXT_shared_texture_palette reported, but glColorTableEXT will not be called from gl_sgis.c.\n");
        // Con_Printf(PRINT_DEVELOPER, "IRIX: is8bit remains false.\n");
        fprintf(stderr, "DEBUG IRIX: GL_EXT_shared_texture_palette reported, but glColorTableEXT will not be called from gl_sgis.c.\n");
        fprintf(stderr, "DEBUG IRIX: is8bit remains false.\n");
        
        // is8bit är redan false och förblir false.
        // Ingen anledning att ha #ifdef IRIX här eftersom hela denna fil är IRIX-specifik.
        // Den tidigare #else-grenen (is8bit = true;) skulle inte heller nås.

    } else {
        Con_SafePrintf("GL_EXT_shared_texture_palette not found. 8-bit GL extensions disabled.\n");
        // is8bit är redan false och förblir false.
    }
}

void VID_Init(unsigned char *palette)
{
    int i;
    char	gldir[MAX_OSPATH];
    int width = 640, height = 480;
    int attrib[] = { 
        GLX_RGBA,
        GLX_RED_SIZE, 8, // SGI stöder ofta 24-bitars färg, så vi kräver 8 bitar per kanal
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_DOUBLEBUFFER,
        GLX_DEPTH_SIZE, 24, // SGI stöder 24-bitars z-buffer
        None };
    int scrnum;
    XSetWindowAttributes attr;
    unsigned long mask;
    Window root;
    XVisualInfo *visinfo;

    S_Init();

    Cvar_RegisterVariable (&vid_mode);
    Cvar_RegisterVariable (&gl_ztrick);
    
    vid.maxwarpwidth = WARP_WIDTH;
    vid.maxwarpheight = WARP_HEIGHT;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

// interpret command-line params

// set vid parameters

    if ((i = COM_CheckParm("-width")) != 0)
        width = atoi(com_argv[i+1]);
    if ((i = COM_CheckParm("-height")) != 0)
        height = atoi(com_argv[i+1]);

    if ((i = COM_CheckParm("-conwidth")) != 0)
        vid.conwidth = Q_atoi(com_argv[i+1]);
    else
        vid.conwidth = 640;

    vid.conwidth &= 0xfff8; // make it a multiple of eight

    if (vid.conwidth < 320)
        vid.conwidth = 320;

    // pick a conheight that matches with correct aspect
    vid.conheight = vid.conwidth*3 / 4;

    if ((i = COM_CheckParm("-conheight")) != 0)
        vid.conheight = Q_atoi(com_argv[i+1]);
    if (vid.conheight < 200)
        vid.conheight = 200;

    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "Error couldn't open the X display\n");
        exit(1);
    }

    scrnum = DefaultScreen(dpy);
    root = RootWindow(dpy, scrnum);

    // På IRIX, använd bästa tillgängliga visuella för bästa prestanda
    visinfo = glXChooseVisual(dpy, scrnum, attrib);
    if (!visinfo) {
        // Försök med mindre krav ifall 24-bitars djup inte är tillgängligt
        attrib[11] = 16; // Minska djupbuffert till 16 bitar
        visinfo = glXChooseVisual(dpy, scrnum, attrib);
        if (!visinfo) {
            fprintf(stderr, "Error: couldn't get an RGB, Double-buffered, Depth visual\n");
            exit(1);
        }
    }

    /* window attributes */
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
    attr.event_mask = KEY_MASK | MOUSE_MASK | VisibilityChangeMask;
    mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

    win = XCreateWindow(dpy, root, 0, 0, width, height,
            0, visinfo->depth, InputOutput,
            visinfo->visual, mask, &attr);
            
    XMapWindow(dpy, win);
    XMoveWindow(dpy, win, 0, 0);
    XFlush(dpy);

    // Ingen MESA_GLX_FX på IRIX, använd direkt IRIS GL
    ctx = glXCreateContext(dpy, visinfo, NULL, True);

    if (!ctx) {
        fprintf(stderr, "Unable to create GLX context.\n");
        exit(1);
    }

    glXMakeCurrent(dpy, win, ctx);

    scr_width = width;
    scr_height = height;

    if (vid.conheight > height)
        vid.conheight = height;
    if (vid.conwidth > width)
        vid.conwidth = width;
    vid.width = vid.conwidth;
    vid.height = vid.conheight;

    vid.aspect = ((float)vid.height / (float)vid.width) *
                (320.0 / 240.0);
    vid.numpages = 2;

    InitSig(); // trap evil signals

    GL_Init();

    sprintf(gldir, "%s/glquake", com_gamedir);
    Sys_mkdir(gldir);

    VID_SetPalette(palette);

    // Kontrollera SGI-specifika texturtillägg
    VID_Init8bitPalette();

    Con_SafePrintf("Video mode %dx%d initialized.\n", width, height);

    vid.recalc_refdef = 1;				// force a surface cache flush
}

void Sys_SendKeyEvents(void)
{
    if (dpy)
    {
        while (Keyboard_Update())
            ;

        while (keyq_head != keyq_tail)
        {
            Key_Event(keyq[keyq_tail].key, keyq[keyq_tail].down);
            keyq_tail = (keyq_tail + 1) & 63;
        }
    }
}

void Force_CenterView_f (void)
{
    cl.viewangles[PITCH] = 0;
}


void IN_Init(void)
{
    Cvar_RegisterVariable(&_windowed_mouse);
    Cvar_RegisterVariable(&m_filter);
    Cvar_RegisterVariable(&mouse_button_commands[0]);
    Cvar_RegisterVariable(&mouse_button_commands[1]);
    Cvar_RegisterVariable(&mouse_button_commands[2]);
    Cmd_AddCommand("force_centerview", Force_CenterView_f);
}

void IN_Shutdown(void)
{
}

/*
===========
IN_Commands
===========
*/
void IN_Commands (void)
{
    int i;

    for (i=0; i<mouse_buttons; i++) {
        if ((mouse_buttonstate & (1<<i)) && !(mouse_oldbuttonstate & (1<<i)))
            Key_Event(K_MOUSE1 + i, true);

        if (!(mouse_buttonstate & (1<<i)) && (mouse_oldbuttonstate & (1<<i)))
            Key_Event(K_MOUSE1 + i, false);
    }

    mouse_oldbuttonstate = mouse_buttonstate;
}

/*
===========
IN_Move
===========
*/
void IN_Move (usercmd_t *cmd)
{
    while (Mouse_Update())
        ;

    if (m_filter.value) {
        mouse_x = (mouse_x + old_mouse_x) * 0.5;
        mouse_y = (mouse_y + old_mouse_y) * 0.5;
    }

    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;
   
    mouse_x *= sensitivity.value;
    mouse_y *= sensitivity.value;
   
    if ((in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1)))
        cmd->sidemove += m_side.value * mouse_x;
    else
        cl.viewangles[YAW] -= m_yaw.value * mouse_x;
    if (in_mlook.state & 1)
        V_StopPitchDrift();
   
    if ((in_mlook.state & 1) && !(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * mouse_y;
        if (cl.viewangles[PITCH] > 80)
            cl.viewangles[PITCH] = 80;
        if (cl.viewangles[PITCH] < -70)
            cl.viewangles[PITCH] = -70;
    } else {
        if ((in_strafe.state & 1) && noclip_anglehack)
            cmd->upmove -= m_forward.value * mouse_y;
        else
            cmd->forwardmove -= m_forward.value * mouse_y;
    }
    mouse_x = mouse_y = 0.0;
}


void VID_LockBuffer (void) {}
void VID_UnlockBuffer (void) {}

/*
void Your_OpenGL_Extension_And_Init_Function(void) // Ge funktionen ett passande namn
{
    // ... (annan GL-initieringskod) ...

    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
    gl_mtexable = false; // Börja med att anta att det inte finns

    if (extensions && strstr(extensions, "GL_SGIS_multitexture"))
    {
        // För IRIX och GL_SGIS_multitexture är funktionerna oftast direkt tillgängliga
        // om systemets GL-bibliotek och headers stöder extensionen.
        // Inget behov av glXGetProcAddress eller liknande för SGI-specifika extensions.
        qglSelectTextureSGIS = glSelectTextureSGIS;
        qglMTexCoord2fSGIS = glMTexCoord2fSGIS; // Kontrollera det exakta namnet på denna funktion i IRIX GL-headers

        if (qglSelectTextureSGIS && qglMTexCoord2fSGIS)
        {
            gl_mtexable = true;
            Con_Printf("Using GL_SGIS_multitexture.\n");
        }
        else
        {
            Con_Printf("GL_SGIS_multitexture reported, but function pointers are NULL. Multitexture disabled.\n");
            // Säkerställ att de är NULL om de inte kunde sättas (även om det är osannolikt om extensionen finns)
            qglSelectTextureSGIS = NULL;
            qglMTexCoord2fSGIS = NULL;
        }
    }
    else
    {
        Con_Printf("GL_SGIS_multitexture not found. Multitexture disabled.\n");
        qglSelectTextureSGIS = NULL;
        qglMTexCoord2fSGIS = NULL;
    }

    // ... (initiera andra extensions och GL-tillstånd) ...
}
*/