/*
 * $Id: patest_record.c,v 1.2.4.4 2003/04/16 19:07:56 philburk Exp $
 * patest_record.c
 * Record input into an array.
 * Optionally save array to a file.
 * Playback recorded data.
 *
 * Author: Phil Burk  http://www.softsynth.com
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include "portaudio.h"

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE  (8000)
#define NUM_MSECONDS     (5000)
#define NUM_CHANNELS    (2)
/* #define DITHER_FLAG     (paDitherOff)  */
#define DITHER_FLAG     (0) /**/
#define FRAMES_PER_BUFFER  (2048)

/* Select sample format. */
#if 1
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#elif 0
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#elif 0
#define PA_SAMPLE_TYPE  paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE  (0)
#else
#define PA_SAMPLE_TYPE  paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE  (128)

#endif

typedef struct
{
    int          frameIndex;  /* Index into sample array. */
    int          maxFrameIndex;
    SAMPLE      *recordedSamples;
}
paSampleData;

typedef struct
{
	paSampleData *Samples[2];
	int RecData;
	int PlayData;

	int record;
	int play;

	int finish;
}
paTestData;

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int Callback( void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           PaTimestamp outTime, void *userData )
{
    paTestData *data = (paTestData*)userData;

    if(data->record && data->RecData != data->PlayData)
    {/* Record */

    	SAMPLE *rptr = (SAMPLE*)inputBuffer;
	paSampleData *RecData = data->Samples[data->RecData];

    	SAMPLE *wptr = &RecData->recordedSamples[RecData->frameIndex * NUM_CHANNELS];

    	long framesToRecord;
    	long i;

    	unsigned long framesLeft = RecData->maxFrameIndex - RecData->frameIndex;
    	int samplesToRecord;

    	if( framesLeft < framesPerBuffer )
    	{
        	framesToRecord = framesLeft;
		data->RecData = (data->RecData+1)%2;
		RecData->frameIndex = 0;
        	data->play = 1;
    	}
    	else
    	{
        	framesToRecord = framesPerBuffer;
    	}
    
    	samplesToRecord = framesToRecord * NUM_CHANNELS;
    
    	if( inputBuffer == NULL )
    	{
        	for( i=0; i<samplesToRecord; i++ )
        	{
            		*wptr++ = SAMPLE_SILENCE;
        	}
    	}
    	else
    	{
        	for( i=0; i<samplesToRecord; i++ )
        	{
            		*wptr++ = *rptr++;
        	}
    	}

    	RecData->frameIndex += framesToRecord;
    }

    /* Playback */
    if(data->play)
    {
    	SAMPLE *wptr = (SAMPLE*)outputBuffer;
	paSampleData *PlayData = data->Samples[data->PlayData];

    	SAMPLE *rptr = &PlayData->recordedSamples[PlayData->frameIndex * NUM_CHANNELS];

    	unsigned int i;
    	unsigned int framesLeft = PlayData->maxFrameIndex - PlayData->frameIndex;

    	int framesToPlay, samplesToPlay, samplesPerBuffer;

    	if( framesLeft < framesPerBuffer )
    	{
        	framesToPlay = framesLeft;
		data->PlayData = (data->PlayData+1)%2;
		PlayData->frameIndex = 0;
    	}
    	else
    	{
        	framesToPlay = framesPerBuffer;
    	}
    
    	samplesToPlay = framesToPlay * NUM_CHANNELS;
    	samplesPerBuffer = framesPerBuffer * NUM_CHANNELS;

    	for( i=0; i<samplesToPlay; i++ )
    	{
        	*wptr++ = *rptr;
            	*rptr++ = SAMPLE_SILENCE;
    	}
    	for( ; i<framesPerBuffer; i++ )
    	{
        	*wptr++ = 0;  /* left */
        	if( NUM_CHANNELS == 2 ) 
			*wptr++ = 0;  /* right */
    	}

    	PlayData->frameIndex += framesToPlay;
    }

    return data->finish;
}

/*******************************************************************/
int main(void)
{
    PortAudioStream *stream;
    PaError    err;

    paTestData data;
    paSampleData Data0;
    paSampleData Data1;

    int        i;
    int        totalFrames;
    int        numSamples;
    int        numBytes;
    SAMPLE     max, average, val;
    printf("patest_record.c\n"); fflush(stdout);

    Data0.maxFrameIndex = totalFrames = NUM_MSECONDS * SAMPLE_RATE / 1000; /* Record for a few milli seconds. */
    Data0.frameIndex = 0;
    numSamples = totalFrames * NUM_CHANNELS;

    numBytes = numSamples * sizeof(SAMPLE);
    Data0.recordedSamples = (SAMPLE *) malloc( numBytes );
    if( Data0.recordedSamples == NULL )
    {
        printf("Could not allocate record array.\n");
        exit(1);
    }
    for( i=0; i<numSamples; i++ ) Data0.recordedSamples[i] = 0;

    Data1.maxFrameIndex = totalFrames = NUM_MSECONDS * SAMPLE_RATE / 1000; /* Record for a few milli seconds. */
    Data1.frameIndex = 0;
    numSamples = totalFrames * NUM_CHANNELS;

    numBytes = numSamples * sizeof(SAMPLE);
    Data1.recordedSamples = (SAMPLE *) malloc( numBytes );
    if( Data1.recordedSamples == NULL )
    {
        printf("Could not allocate record array.\n");
        exit(1);
    }
    for( i=0; i<numSamples; i++ ) Data1.recordedSamples[i] = 0;

    data.Samples[0] = &Data0;
    data.Samples[1] = &Data1;
    data.RecData = 0;
    data.PlayData = 1;
    data.record = 1;
    data.play = 0;
    data.finish = 0;

    err = Pa_Initialize();
    if( err != paNoError ) goto error;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
              &stream,
              Pa_GetDefaultInputDeviceID(),
              NUM_CHANNELS,
              PA_SAMPLE_TYPE, /* Input */
              NULL,
              Pa_GetDefaultOutputDeviceID(),
              NUM_CHANNELS,
              PA_SAMPLE_TYPE, /* Output */
              NULL,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,            /* frames per buffer */
              0,               /* number of buffers, if zero then use default minimum */
              0, /* paDitherOff, // flags */
              Callback,
              &data );

    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;
    printf("Now recording!!\n"); fflush(stdout);

    while( Pa_StreamActive( stream ) )
    {
        Pa_Sleep(1000);
	if(data.record)
	{
        	printf("recindex = %d\n", data.Samples[data.RecData]->frameIndex ); fflush(stdout);
  	}
	else
	{      
		printf("playindex = %d\n", data.Samples[data.PlayData]->frameIndex ); fflush(stdout);
	}
    }

#if 0
    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error;
    /* Measure maximum peak amplitude. */
    max = 0;
    average = 0;
    for( i=0; i<numSamples; i++ )
    {
        val = data.recordedSamples[i];
        if( val < 0 ) val = -val; /* ABS */
        if( val > max )
        {
            max = val;
        }
        average += val;
    }
    
    average = average / numSamples;

    if( PA_SAMPLE_TYPE == paFloat32 )   /* This should be done at compile-time with "#if" ?? */
    {                                   /* MIPS-compiler warns at the int-version below.     */
        printf("sample max amplitude = %f\n", max );
        printf("sample average = %f\n", average );
    }
    else
    {
        printf("sample max amplitude = %d\n", max );    /* <-- This IS compiled anyhow. */
        printf("sample average = %d\n", average );
    }
#endif    
    /* Write recorded data to a file. */
#if 0
    {
        FILE  *fid;
        fid = fopen("recorded.raw", "wb");
        if( fid == NULL )
        {
            printf("Could not open file.");
        }
        else
        {
            fwrite( data.recordedSamples, NUM_CHANNELS * sizeof(SAMPLE), totalFrames, fid );
            fclose( fid );
            printf("Wrote data to 'recorded.raw'\n");
        }
    }
#endif
#if 0
    /* Playback recorded data.  -------------------------------------------- */
    data.frameIndex = 0;
    printf("Begin playback.\n"); fflush(stdout);
    err = Pa_OpenStream(
              &stream,
              paNoDevice,
              0,               /* NO input */
              PA_SAMPLE_TYPE,
              NULL,
              Pa_GetDefaultOutputDeviceID(),
              NUM_CHANNELS,
              PA_SAMPLE_TYPE,
              NULL,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,            /* frames per buffer */
              0,               /* number of buffers, if zero then use default minimum */
              paClipOff,       /* we won't output out of range samples so don't bother clipping them */
              playCallback,
              &data );
    if( err != paNoError ) goto error;

    if( stream )
    {
        err = Pa_StartStream( stream );
        if( err != paNoError ) goto error;
        printf("Waiting for playback to finish.\n"); fflush(stdout);

        while( Pa_StreamActive( stream ) ) Pa_Sleep(100);

        err = Pa_CloseStream( stream );
        if( err != paNoError ) goto error;
        printf("Done.\n"); fflush(stdout);
    }
    free( data.recordedSamples );
#endif
    Pa_Terminate();
    return 0;

error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return -1;
}
