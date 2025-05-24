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

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef IRIX
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/scsi.h>
#include <dmedia/cdaudio.h>
// Define the CD success status since it's not in the header
#define CD_SUCCESS 0
#else
#include <linux/cdrom.h>
#endif

#include "quakedef.h"

static qboolean cdValid = false;
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	initialized = false;
static qboolean	enabled = true;
static qboolean playLooping = false;
static float	cdvolume;
static byte 	remap[100];
static byte		playTrack;
static byte		maxTrack;

#ifdef IRIX
// For IRIX
static CDPLAYER *cdp = NULL;
static CDSTATUS cdstat;
static CDTRACKINFO trackinfo;

// IRIX CD playing modes
#ifndef CD_TRACK_NORMAL
#define CD_TRACK_NORMAL 0
#endif
#ifndef CD_TRACK_REPEAT
#define CD_TRACK_REPEAT 1
#endif

// Define IRIX CD states as per dmedia/cdaudio.h
// We're commenting out the defines in case they're already defined
// but creating our own constants to use in the code
// These match the values in IRIX's dmedia/cdaudio.h
#define IRIX_CD_READY 1
#define IRIX_CD_PLAYING 2
#define IRIX_CD_PAUSED 3
#define IRIX_CD_STOPPED 4

#else
// For Linux
static int cdfile = -1;
static struct cdrom_tochdr cdtochdr;
#endif

static char cd_dev[64] = "/dev/cdrom";

static void CDAudio_Eject(void)
{
    if (!enabled)
        return;

#ifdef IRIX
    if (cdp != NULL) {
        if (CDeject(cdp) != CD_SUCCESS)
            Con_DPrintf("CDaudio: eject failed\n");
    }
#else
    if (cdfile == -1)
        return;

    if (ioctl(cdfile, CDROMEJECT) == -1) 
        Con_DPrintf("CDAudio: eject failed\n");
#endif
}

static void CDAudio_CloseDoor(void)
{
#ifdef IRIX
    // Try to close door if IRIX supports it
    if (!enabled || !cdp)
        return;

    // Try close tray - not all IRIX versions support this
    if (CDgetstatus(cdp, &cdstat) == CD_SUCCESS) {
        Con_Printf("CD state: %d\n", cdstat.state);
        if (cdstat.state == IRIX_CD_STOPPED) {
            // Door might be open, try toggling it
            if (CDtoggledoor(cdp) != CD_SUCCESS)
                Con_DPrintf("CDaudio: close door failed\n");
        }
    }
#else
    if (cdfile == -1)
        return;
        
    if (ioctl(cdfile, CDROMCLOSETRAY) == -1)
        Con_DPrintf("CDAudio: close tray failed\n");
#endif
}

static int CDAudio_GetAudioDiskInfo(void)
{
#ifdef IRIX
    int i;
    
    cdValid = false;
    
    if (!cdp)
        return -1;
        
    // Update status
    if (CDgetstatus(cdp, &cdstat) != CD_SUCCESS) {
        Con_DPrintf("CDaudio: status read failed\n");
        return -1;
    }
    
    if (cdstat.state == IRIX_CD_READY || cdstat.state == IRIX_CD_PAUSED || 
        cdstat.state == IRIX_CD_PLAYING || cdstat.state == IRIX_CD_STOPPED) {
        // Get the track count
        maxTrack = 0;
        for (i = 1; i <= 99; i++) {
            if (CDgettrackinfo(cdp, i, &trackinfo) == CD_SUCCESS) {
                maxTrack = i;
            } else {
                break;
            }
        }
        
        if (maxTrack >= 1) {
            cdValid = true;
            return 0;
        }
    }
    
    Con_DPrintf("CDaudio: no music tracks\n");
    return -1;
#else
    cdValid = false;

    if (cdfile == -1)
        return -1;

    if (ioctl(cdfile, CDROMREADTOCHDR, &cdtochdr) == -1) {
        Con_DPrintf("CDAudio: readtoc failed\n");
        return -1;
    }

    if (cdtochdr.cdth_trk0 < 1) {
        Con_DPrintf("CDAudio: no music tracks\n");
        return -1;
    }

    cdValid = true;
    maxTrack = cdtochdr.cdth_trk1;

    return 0;
#endif
}

void CDAudio_Play(byte track, qboolean looping)
{
#ifdef IRIX
    if (!enabled || !cdp)
        return;
    
    if (!cdValid) {
        CDAudio_GetAudioDiskInfo();
        if (!cdValid)
            return;
    }

    track = remap[track];

    if (track < 1 || track > maxTrack) {
        Con_DPrintf("CDaudio: Bad track number %u.\n", track);
        return;
    }

    // Check if it's an audio track
    if (CDgettrackinfo(cdp, track, &trackinfo) != CD_SUCCESS) {
        Con_DPrintf("CDaudio: track info failed\n");
        return;
    }
    
    // Assuming all tracks are audio for now - IRIX API may vary
    
    if (playing) {
        if (playTrack == track)
            return;
        CDAudio_Stop();
    }

    // Play the track
    if (CDplaytrack(cdp, track, 
                   looping ? CD_TRACK_REPEAT : CD_TRACK_NORMAL) != CD_SUCCESS) {
        Con_DPrintf("CDaudio: play track failed\n");
        return;
    }

    playLooping = looping;
    playTrack = track;
    playing = true;

    if (cdvolume == 0.0)
        CDAudio_Pause();
#else
    struct cdrom_tocentry entry;
    struct cdrom_ti ti;

    if (!enabled)
        return;
        
    if (!cdValid) {
        CDAudio_GetAudioDiskInfo();
        if (!cdValid)
            return;
    }

    track = remap[track];

    if (track < 1 || track > maxTrack) {
        Con_DPrintf("CDaudio: Bad track number %u.\n", track);
        return;
    }

    // don't try to play a non-audio track
    entry.cdte_track = track;
    entry.cdte_format = CDROM_MSF;
    if (ioctl(cdfile, CDROMREADTOCENTRY, &entry) == -1) {
        Con_DPrintf("CDAudio: ioctl read failed\n");
        return;
    }
    if (entry.cdte_ctrl & CDROM_DATA_TRACK) {
        Con_Printf("CDAudio: track %i is not audio\n", track);
        return;
    }

    if (playing) {
        if (playTrack == track)
            return;
        CDAudio_Stop();
    }
    
    ti.cdti_trk0 = track;
    ti.cdti_trk1 = track;
    ti.cdti_ind0 = 1;
    ti.cdti_ind1 = 99;

    if (ioctl(cdfile, CDROMPLAYTRKIND, &ti) == -1) {
        Con_DPrintf("CDAudio: ioctl play failed\n");
        return;
    }

    if (ioctl(cdfile, CDROMRESUME) == -1)
        Con_DPrintf("CDAudio: ioctl resume failed\n");

    playLooping = looping;
    playTrack = track;
    playing = true;

    if (cdvolume == 0.0)
        CDAudio_Pause();
#endif
}

void CDAudio_Stop(void)
{
#ifdef IRIX
    if (!enabled || !cdp)
        return;
    
    if (!playing)
        return;

    // IRIX stop command
    if (CDstop(cdp) != CD_SUCCESS)
        Con_DPrintf("CDaudio: stop failed\n");

    wasPlaying = false;
    playing = false;
#else
    if (!enabled)
        return;
        
    if (cdfile == -1)
        return;

    if (!playing)
        return;

    if (ioctl(cdfile, CDROMSTOP) == -1)
        Con_DPrintf("CDAudio: ioctl stop failed\n");

    wasPlaying = false;
    playing = false;
#endif
}

void CDAudio_Pause(void)
{
#ifdef IRIX
    if (!enabled || !cdp)
        return;

    if (!playing)
        return;

    // IRIX pause command
    if (CDpause(cdp) != CD_SUCCESS)
        Con_DPrintf("CDaudio: pause failed\n");

    wasPlaying = playing;
    playing = false;
#else
    if (!enabled)
        return;

    if (cdfile == -1)
        return;

    if (!playing)
        return;

    if (ioctl(cdfile, CDROMPAUSE) == -1) 
        Con_DPrintf("CDAudio: ioctl pause failed\n");

    wasPlaying = playing;
    playing = false;
#endif
}

void CDAudio_Resume(void)
{
#ifdef IRIX
    if (!enabled || !cdp)
        return;
    
    if (!cdValid)
        return;

    if (!wasPlaying)
        return;
    
    // IRIX resume command
    if (CDresume(cdp) != CD_SUCCESS)
        Con_DPrintf("CDaudio: resume failed\n");
    
    playing = true;
#else
    if (!enabled)
        return;
        
    if (cdfile == -1)
        return;
        
    if (!cdValid)
        return;

    if (!wasPlaying)
        return;
    
    if (ioctl(cdfile, CDROMRESUME) == -1) 
        Con_DPrintf("CDAudio: ioctl resume failed\n");
    playing = true;
#endif
}

static void CD_f(void)
{
    char    *command;
    int     ret;
    int     n;

    if (Cmd_Argc() < 2)
        return;

    command = Cmd_Argv(1);

    if (Q_strcasecmp(command, "on") == 0) {
        enabled = true;
        return;
    }

    if (Q_strcasecmp(command, "off") == 0) {
        if (playing)
            CDAudio_Stop();
        enabled = false;
        return;
    }

    if (Q_strcasecmp(command, "reset") == 0) {
        enabled = true;
        if (playing)
            CDAudio_Stop();
        for (n = 0; n < 100; n++)
            remap[n] = n;
        CDAudio_GetAudioDiskInfo();
        return;
    }

    if (Q_strcasecmp(command, "remap") == 0) {
        ret = Cmd_Argc() - 2;
        if (ret <= 0) {
            for (n = 1; n < 100; n++)
                if (remap[n] != n)
                    Con_Printf("  %u -> %u\n", n, remap[n]);
            return;
        }
        for (n = 1; n <= ret; n++)
            remap[n] = Q_atoi(Cmd_Argv(n+1));
        return;
    }

    if (Q_strcasecmp(command, "close") == 0) {
        CDAudio_CloseDoor();
        return;
    }

    if (!cdValid) {
        CDAudio_GetAudioDiskInfo();
        if (!cdValid) {
            Con_Printf("No CD in player.\n");
            return;
        }
    }

    if (Q_strcasecmp(command, "play") == 0) {
        CDAudio_Play((byte)Q_atoi(Cmd_Argv(2)), false);
        return;
    }

    if (Q_strcasecmp(command, "loop") == 0) {
        CDAudio_Play((byte)Q_atoi(Cmd_Argv(2)), true);
        return;
    }

    if (Q_strcasecmp(command, "stop") == 0) {
        CDAudio_Stop();
        return;
    }

    if (Q_strcasecmp(command, "pause") == 0) {
        CDAudio_Pause();
        return;
    }

    if (Q_strcasecmp(command, "resume") == 0) {
        CDAudio_Resume();
        return;
    }

    if (Q_strcasecmp(command, "eject") == 0) {
        if (playing)
            CDAudio_Stop();
        CDAudio_Eject();
        cdValid = false;
        return;
    }

    if (Q_strcasecmp(command, "info") == 0) {
        Con_Printf("%u tracks\n", maxTrack);
        if (playing)
            Con_Printf("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
        else if (wasPlaying)
            Con_Printf("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);
        Con_Printf("Volume is %f\n", cdvolume);
        return;
    }
}

void CDAudio_Update(void)
{
#ifdef IRIX
    static time_t lastchk;

    if (!enabled || !cdp)
        return;

    if (bgmvolume.value != cdvolume) {
        if (cdvolume) {
            Cvar_SetValue("bgmvolume", 0.0);
            cdvolume = bgmvolume.value;
            CDAudio_Pause();
        } else {
            Cvar_SetValue("bgmvolume", 1.0);
            cdvolume = bgmvolume.value;
            CDAudio_Resume();
        }
    }

    if (playing && lastchk < time(NULL)) {
        lastchk = time(NULL) + 2; // Two seconds between checks

        // Update CD player status
        if (CDgetstatus(cdp, &cdstat) != CD_SUCCESS) {
            Con_DPrintf("CDaudio: status check failed\n");
            playing = false;
            return;
        }

        // Check if playback is finished
        if (cdstat.state != IRIX_CD_PLAYING) {
            playing = false;
            if (playLooping)
                CDAudio_Play(playTrack, true);
        }
    }
#else
    struct cdrom_subchnl subchnl;
    static time_t lastchk;

    if (!enabled)
        return;

    if (cdfile == -1)
        return;

    if (bgmvolume.value != cdvolume) {
        if (cdvolume) {
            Cvar_SetValue("bgmvolume", 0.0);
            cdvolume = bgmvolume.value;
            CDAudio_Pause();
        } else {
            Cvar_SetValue("bgmvolume", 1.0);
            cdvolume = bgmvolume.value;
            CDAudio_Resume();
        }
    }

    if (playing && lastchk < time(NULL)) {
        lastchk = time(NULL) + 2; //2 seconds between chks
        subchnl.cdsc_format = CDROM_MSF;
        if (ioctl(cdfile, CDROMSUBCHNL, &subchnl) == -1) {
            Con_DPrintf("CDAudio: subchnl ioctl failed\n");
            playing = false;
            return;
        }
        if (subchnl.cdsc_audiostatus != CDROM_AUDIO_PLAY && subchnl.cdsc_audiostatus != CDROM_AUDIO_PAUSED) {
            playing = false;
            if (playLooping)
                CDAudio_Play(playTrack, true);
        }
    }
#endif
}

int CDAudio_Init(void)
{
    int i;

    if (COM_CheckParm("-nocdaudio"))
        return -1;

    if ((i = COM_CheckParm("-cddev")) != 0 && i < com_argc - 1) {
        strncpy(cd_dev, com_argv[i + 1], sizeof(cd_dev));
        cd_dev[sizeof(cd_dev) - 1] = 0;
    }

#ifdef IRIX
    // Open CD player using dmedia API
    cdp = (CDPLAYER *)CDopenplayer(cd_dev);
    if (cdp == NULL) {
        Con_Printf("CDAudio_Init: Unable to open CD player: %s\n", cd_dev);
        return -1;
    }
#else
    if ((cdfile = open(cd_dev, O_RDONLY | O_NONBLOCK)) == -1) {
        Con_Printf("CDAudio_Init: open of \"%s\" failed (%i)\n", cd_dev, errno);
        cdfile = -1;
        return -1;
    }
#endif

    for (i = 0; i < 100; i++)
        remap[i] = i;
    
    initialized = true;
    enabled = true;

    if (CDAudio_GetAudioDiskInfo()) {
        Con_Printf("CDAudio_Init: No CD in player.\n");
        cdValid = false;
    }

    Cmd_AddCommand("cd", CD_f);

    Con_Printf("CD Audio Initialized\n");

    return 0;
}

void CDAudio_Shutdown(void)
{
    if (!initialized)
        return;
    
    CDAudio_Stop();
    
#ifdef IRIX
    if (cdp) {
        CDcloseplayer(cdp);
        cdp = NULL;
    }
#else
    if (cdfile != -1)
        close(cdfile);
    cdfile = -1;
#endif
}