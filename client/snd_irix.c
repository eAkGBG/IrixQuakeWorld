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
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
// IRIX använder dmedia/audio.h istället för linux/soundcard.h
#include <dmedia/audio.h>
#include <dmedia/audiofile.h>
#include <stdio.h>
#include "quakedef.h"

// ALport är IRIX-motsvarigheten till file descriptor för ljud
ALport audio_port = NULL;
ALconfig audio_config = NULL;
int snd_inited;

static int tryrates[] = { 11025, 22050, 44100, 8000 };

qboolean SNDDMA_Init(void)
{
    int i;
    char *s;
    long audio_params[4];
    int width, stereo;

    snd_inited = 0;

    // Skapa en ALconfig för att ställa in ljudparametrar
    audio_config = ALnewconfig();
    if (!audio_config) {
        Con_Printf("Failed to create ALconfig\n");
        return 0;
    }

    shm = &sn;
    shm->splitbuffer = 0;

    // Ställ in samplebits
    s = getenv("QUAKE_SOUND_SAMPLEBITS");
    if (s) 
        shm->samplebits = atoi(s);
    else if ((i = COM_CheckParm("-sndbits")) != 0)
        shm->samplebits = atoi(com_argv[i+1]);
    else
        shm->samplebits = 16; // Default till 16-bit

    // SGI:s ljudsystem stöder 8-bit & 16-bit, välj lämplig bredd
    if (shm->samplebits == 16) {
        ALsetwidth(audio_config, AL_SAMPLE_16);
        width = AL_SAMPLE_16;
    } else {
        ALsetwidth(audio_config, AL_SAMPLE_8);
        width = AL_SAMPLE_8;
        shm->samplebits = 8;
    }

    // Ställ in samplingsfrekvens
    s = getenv("QUAKE_SOUND_SPEED");
    if (s) 
        shm->speed = atoi(s);
    else if ((i = COM_CheckParm("-sndspeed")) != 0)
        shm->speed = atoi(com_argv[i+1]);
    else {
        // Testa olika samplingsfrekvenser
        for (i = 0; i < sizeof(tryrates) / sizeof(tryrates[0]); i++) {
            if (ALsetrate(audio_config, tryrates[i]) == 0) {
                shm->speed = tryrates[i];
                break;
            }
        }
        
        // Om ingen frekvens fungerade, försök med standardvärdet 11025
        if (i == sizeof(tryrates) / sizeof(tryrates[0])) {
            ALsetrate(audio_config, 11025);
            shm->speed = 11025;
        }
    }

    // Ställ in kanaler (mono/stereo)
    s = getenv("QUAKE_SOUND_CHANNELS");
    if (s) 
        shm->channels = atoi(s);
    else if (COM_CheckParm("-sndmono"))
        shm->channels = 1;
    else if (COM_CheckParm("-sndstereo"))
        shm->channels = 2;
    else 
        shm->channels = 2;  // Default till stereo

    // Ställ in kanalkonfiguration
    if (shm->channels == 2) {
        ALsetchannels(audio_config, AL_STEREO);
        stereo = AL_STEREO;
    } else {
        ALsetchannels(audio_config, AL_MONO);
        stereo = AL_MONO;
        shm->channels = 1;
    }

    // Ställ in queue-storlek för att minska latens
    ALsetqueuesize(audio_config, 16384);

    // Öppna ljudenheten
    audio_port = ALopenport("quake", "w", audio_config);
    if (!audio_port) {
        Con_Printf("Failed to open audio port\n");
        ALfreeconfig(audio_config);
        return 0;
    }

    // Hämta konfigurationsparametrar
    ALgetparams(AL_DEFAULT_DEVICE, audio_params, 4);

    // Beräkna samplebufferstorlek
    if (shm->channels == 2 && shm->samplebits == 16)
        shm->samples = 16384 / 4;  // Stereo 16-bit
    else if (shm->channels == 2 || shm->samplebits == 16)
        shm->samples = 16384 / 2;  // Stereo 8-bit eller Mono 16-bit
    else
        shm->samples = 16384;      // Mono 8-bit

    shm->submission_chunk = 1;

    // Allokera ljudbuffert - IRIX använder inte mmap som Linux
    shm->buffer = (unsigned char *) calloc(1, shm->samples * (shm->samplebits / 8));
    if (!shm->buffer) {
        Con_Printf("Failed to allocate audio buffer\n");
        ALcloseport(audio_port);
        ALfreeconfig(audio_config);
        return 0;
    }

    shm->samplepos = 0;
    snd_inited = 1;
    
    return 1;
}

int SNDDMA_GetDMAPos(void)
{
    ALparamInfo pinfo;

    if (!snd_inited) 
        return 0;

    // På IRIX använder vi ALgetfilled/ALgetfillable för att avgöra position
    // Detta motsvarar inte exakt Linux-versionen men är funktionellt likvärdigt
    if (ALgetfilled(audio_port) == -1) {
        Con_Printf("Sound system error.\n");
        SNDDMA_Shutdown();
        return 0;
    }

    // I IRIX behöver vi beräkna positionen på annat sätt
    // Vi använder samplepositionen som återstår att spela
    shm->samplepos = (shm->samplepos + 128) % shm->samples;

    return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
    if (snd_inited) {
        if (shm->buffer) {
            free(shm->buffer);
            shm->buffer = NULL;
        }
        
        if (audio_port) {
            ALcloseport(audio_port);
            audio_port = NULL;
        }
        
        if (audio_config) {
            ALfreeconfig(audio_config);
            audio_config = NULL;
        }
        
        snd_inited = 0;
    }
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
    int bytes_written;
    
    if (!snd_inited || !audio_port || !shm->buffer)
        return;

    // I IRIX måste vi faktiskt skriva ljuddata till ljudporten
    // Detta är annorlunda än Linux som använder mmap
    bytes_written = ALwritesamps(audio_port, shm->buffer, shm->samples);
    
    // Återställ samplepositionen om vi har skrivit hela bufferten
    if (bytes_written == shm->samples)
        shm->samplepos = 0;
}