/*

	Open AL Audio driver

*/

#if !DISABLE_AUDIO

#if OS_APPLE
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include <stdio.h>
#include <stdint.h>

#include "logfile.h"

#define NUMBUFFERS            (3)				/* living dangerously*/

ALuint		  uiBuffers[NUMBUFFERS];
ALuint		  uiSource;

ALboolean ALFWInitOpenAL()
{
	ALCcontext *pContext = NULL;
	ALCdevice *pDevice = NULL;
	ALboolean bReturn = AL_FALSE;
	
	pDevice = alcOpenDevice(NULL);				/* Request default device*/
	if (pDevice)
	{
		pContext = alcCreateContext(pDevice, NULL);
		if (pContext)
		{
			CONSOLE_OUTPUT("\nOpened %s Device\n", alcGetString(pDevice, ALC_DEVICE_SPECIFIER));
			alcMakeContextCurrent(pContext);
			bReturn = AL_TRUE;
		}
		else
		{
			alcCloseDevice(pDevice);
		}
	}

	return bReturn;
}

ALboolean ALFWShutdownOpenAL()
{
	ALCcontext *pContext;
	ALCdevice *pDevice;

	pContext = alcGetCurrentContext();
	pDevice = alcGetContextsDevice(pContext);
	
	alcMakeContextCurrent(NULL);
	alcDestroyContext(pContext);
	alcCloseDevice(pDevice);

	return AL_TRUE;
}

#if 0/*USE_8BIT_OUTPUT*/

#define AL_FORMAT						(AL_FORMAT_MONO8)
#define BUFFER_FORMAT				U8
#define BUFFER_FORMAT_SIZE	(1)
#define BUFFER_FORMAT_SHIFT	(8)

#else

#define AL_FORMAT						(AL_FORMAT_STEREO16)
#define BUFFER_FORMAT				int16_t
#define BUFFER_FORMAT_SIZE	(4)
#define BUFFER_FORMAT_SHIFT	(0)

#endif

int curPlayBuffer=0;

#define BUFFER_LEN		(44100/50)

BUFFER_FORMAT audioBuffer[BUFFER_LEN*2];
int amountAdded=0;

int16_t currentDAC[2] = {0,0};
uint32_t tickCnt=0;
uint32_t tickRate;

void AudioInitialise(unsigned int tick)
{
	int a=0;

	tickRate=((tick*4096)/(44100/50));			// 1 frame == 1 44100 hz audio

	for (a=0;a<BUFFER_LEN;a++)
	{
		audioBuffer[a]=0;
	}

	ALFWInitOpenAL();

  /* Generate some AL Buffers for streaming */
	alGenBuffers( NUMBUFFERS, uiBuffers );

	/* Generate a Source to playback the Buffers */
  alGenSources( 1, &uiSource );

	for (a=0;a<NUMBUFFERS;a++)
	{
		alBufferData(uiBuffers[a], AL_FORMAT, audioBuffer, BUFFER_LEN*BUFFER_FORMAT_SIZE, 44100);
		alSourceQueueBuffers(uiSource, 1, &uiBuffers[a]);
	}

	alSourcePlay(uiSource);
}


void AudioKill()
{
	ALFWShutdownOpenAL();
}

void _AudioAddData(int channel,int16_t dacValue)
{
	currentDAC[channel]=dacValue;
}

/* audio ticked at same clock as everything else... so need a step down */
void AudioUpdate(int numClocks)
{
	tickCnt+=numClocks*4096;
	
	if (tickCnt>=tickRate*50)
	{
		tickCnt-=tickRate;

		if (amountAdded!=BUFFER_LEN*2)
		{
			audioBuffer[amountAdded]=currentDAC[0]>>BUFFER_FORMAT_SHIFT;
			amountAdded++;
			audioBuffer[amountAdded]=currentDAC[1]>>BUFFER_FORMAT_SHIFT;
			amountAdded++;
		}
	}

	if (amountAdded==BUFFER_LEN*2)
	{
		/* 1 second has passed by */

		ALint processed;
		ALint state;
		alGetSourcei(uiSource, AL_SOURCE_STATE, &state);
		alGetSourcei(uiSource, AL_BUFFERS_PROCESSED, &processed);
		if (processed>0)
		{
			ALuint buffer;

			amountAdded=0;
			alSourceUnqueueBuffers(uiSource,1, &buffer);
			alBufferData(buffer, AL_FORMAT, audioBuffer, BUFFER_LEN*BUFFER_FORMAT_SIZE, 44100);
			alSourceQueueBuffers(uiSource, 1, &buffer);
		}

		if (state!=AL_PLAYING)
		{
			alSourcePlay(uiSource);
		}
	}
}

#endif//!DISABLE_AUDIO

