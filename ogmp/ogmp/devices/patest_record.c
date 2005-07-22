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
#include <portaudio.h>
 
#include <timedia/os.h>
#include <string.h>

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE  (44100)
#define NUM_SECONDS     (5)
#define NUM_CHANNELS    (2)
/* #define DITHER_FLAG     (paDitherOff)  */
#define DITHER_FLAG     (0) /**/
#define FRAMES_PER_BUFFER  (1024)

/* Select sample format. */
#if 0
 #define PA_SAMPLE_TYPE  paFloat32
 typedef float SAMPLE;
 #define SAMPLE_SILENCE  (0.0f)
#elif 1
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
    char      *pcm;
    int        nbyte;
}
paSampleData;

typedef struct
{
	int RecData;
	int PlayData;

	int record;
	int play;

	int finish;
   
	paSampleData *Samples;
}
paDevice;

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int recordCallback (const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    paDevice *padev = (paDevice*)userData;
	 paSampleData *RecData = &padev->Samples[padev->RecData];

    SAMPLE *wptr = &RecData->pcm[RecData->frameIndex * NUM_CHANNELS];    
    SAMPLE *rptr = (SAMPLE*)inputBuffer;
    
    long framesToRecord;
    long i;
    int buffer_full = 0;
              
    unsigned long framesLeft = RecData->maxFrameIndex - RecData->frameIndex;
    int samplesToRecord;

    (void) timeInfo;

    if( framesLeft < framesPerBuffer )
    {
        framesToRecord = framesLeft;
        buffer_full = 1;
    }
    else
        framesToRecord = framesPerBuffer;
    
    samplesToRecord = framesToRecord * NUM_CHANNELS;
    
    if( inputBuffer == NULL )
    {
        for( i=0; i<samplesToRecord; i++ )
            *wptr++ = SAMPLE_SILENCE;
    }
    else
    {
        for( i=0; i<samplesToRecord; i++ )
            *wptr++ = *rptr++;
    }
    
    RecData->frameIndex += framesToRecord;
    
    return buffer_full;
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int playCallback (const void *inputBufferr, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData )
{
    paDevice *padev = (paDevice*)userData;

    paSampleData *PlayData = &padev->Samples[padev->PlayData];
    SAMPLE *rptr = &PlayData->pcm[PlayData->frameIndex * NUM_CHANNELS];
    SAMPLE *wptr = (SAMPLE*)outputBuffer;
    
    unsigned int i;
    int buffer_empty = 0;
    
    unsigned int framesLeft = PlayData->maxFrameIndex - PlayData->frameIndex;
    (void) timeInfo;
    
    int framesToPlay, samplesToPlay, samplesPerBuffer;

    if( framesLeft < framesPerBuffer )
    {
        framesToPlay = framesLeft;
        buffer_empty = 1;
    }
    else
        framesToPlay = framesPerBuffer;
    
    samplesToPlay = framesToPlay * NUM_CHANNELS;
    samplesPerBuffer = framesPerBuffer * NUM_CHANNELS;

    for( i=0; i<samplesToPlay; i++ )
    {
        *wptr++ = *rptr++;
    }
    for( ; i<framesPerBuffer; i++ )
    {
        *wptr++ = 0;  /* left */
        if( NUM_CHANNELS == 2 ) *wptr++ = 0;  /* right */
    }
    
    PlayData->frameIndex += framesToPlay;

    return buffer_empty;
}

static int ioCallback (const void *inputBuffer, void *outputBuffer,
                       unsigned long framesPerBuffer,
                       const PaStreamCallbackTimeInfo* timeInfo,
                       PaStreamCallbackFlags statusFlags,
                       void *userData )
{
    paDevice *padev = (paDevice*)userData;

    if(padev->record)
    {
       /* Record */
	    paSampleData *RecData = &padev->Samples[padev->RecData];
      
       int full = recordCallback(inputBuffer, outputBuffer, framesPerBuffer, timeInfo, statusFlags, userData);
       if(full)
       {
		   padev->RecData = (padev->RecData+1)%2;
         RecData = &padev->Samples[padev->RecData];
         
		   RecData->frameIndex = 0;
        	padev->play = 1;

         if(padev->RecData == padev->PlayData)
            padev->record = 0;
       }
    }

    if(padev->play)
    {
       /* Playback */
       paSampleData *PlayData = &padev->Samples[padev->PlayData];
       
       int empty = playCallback(inputBuffer, outputBuffer, framesPerBuffer, timeInfo, statusFlags, userData);
       if(empty)
       {
		   padev->PlayData = (padev->PlayData+1)%2;
         PlayData = &padev->Samples[padev->PlayData];
         
		   PlayData->frameIndex = 0;

        	padev->record = 1;
       }
    }

    return padev->finish;
}

void padev_list_devices()
{
   int hdi;
   PaDeviceIndex di;
   const PaDeviceInfo *devinfo;
   
   PaHostApiIndex hi;
   PaHostApiIndex hosts = Pa_GetHostApiCount ();
   PaHostApiIndex thehost = Pa_GetDefaultHostApi();
   
   const PaHostApiInfo *hostinfo;
   
   printf("\nSystem: %d hostapi\n\r", hosts); fflush(stdout);
   printf("Default host: #%d\n\r", thehost); fflush(stdout);
   printf("Default device: I[#%d] O[#%d]\n\r", Pa_GetDefaultInputDevice(), Pa_GetDefaultOutputDevice()); fflush(stdout);
   printf("=====================================\n\r"); fflush(stdout);
   
   for(hi=0; hi<hosts; hi++)
   {
      hostinfo = Pa_GetHostApiInfo (hi);

      printf("Host#%d: %s, %d devices\n\r", hi, hostinfo->name, hostinfo->deviceCount); fflush(stdout);
      printf("default device:I[#%d], O[#%d]\n\r", hostinfo->defaultInputDevice, hostinfo->defaultOutputDevice); fflush(stdout);

      for(hdi=0; hdi<hostinfo->deviceCount; hdi++)
      {
            di = Pa_HostApiDeviceIndexToDeviceIndex (hi, hdi);
            devinfo = Pa_GetDeviceInfo (di);

            printf("\nDevice#%d: %s, rate %f\n\r", hdi, devinfo->name, devinfo->defaultSampleRate); fflush(stdout);
            printf("max channels: I[%d] O[%d]\n\r", devinfo->maxInputChannels, devinfo->maxOutputChannels); fflush(stdout);
            printf("default low latency: I[%f] O[%f]\n\r", devinfo->defaultLowInputLatency, devinfo->defaultLowOutputLatency); fflush(stdout);
            printf("default high latency: I[%f] O[%f]\n\r", devinfo->defaultHighInputLatency, devinfo->defaultHighOutputLatency); fflush(stdout);
      }
      
      printf("--------------------------------------\n\r"); fflush(stdout);
   }
}

const PaHostApiInfo* padev_find_io(PaHostApiTypeId hostType, PaDeviceIndex *inputDevice, PaDeviceIndex *outputDevice)
{
    PaHostApiIndex hostapi = -1;
    const PaHostApiInfo *hostinfo;

    hostapi = Pa_HostApiTypeIdToHostApiIndex (hostType);
    if(hostapi >= 0)
    {
       int hdi;
       PaDeviceIndex di;
       const PaDeviceInfo *devinfo;

       hostinfo = Pa_GetHostApiInfo (hostapi);


       for(hdi=0; hdi<hostinfo->deviceCount; hdi++)
       {
          di = Pa_HostApiDeviceIndexToDeviceIndex (hostapi, hdi);
          devinfo = Pa_GetDeviceInfo (di);

          printf("\nDevice#%d: %s, rate %f\n\r", hdi, devinfo->name, devinfo->defaultSampleRate); fflush(stdout);
          printf("max channels: I[%d] O[%d]\n\r", devinfo->maxInputChannels, devinfo->maxOutputChannels); fflush(stdout);
          printf("default low latency: I[%f] O[%f]\n\r", devinfo->defaultLowInputLatency, devinfo->defaultLowOutputLatency); fflush(stdout);
          printf("default high latency: I[%f] O[%f]\n\r", devinfo->defaultHighInputLatency, devinfo->defaultHighOutputLatency); fflush(stdout);

          if(devinfo->maxInputChannels > 0 && devinfo->maxOutputChannels > 0)
          {
            printf("Select Audio I/O device#%d: %s\n\r", hdi, devinfo->name); fflush(stdout);
            *inputDevice = hdi;
            *outputDevice = hdi;
             return hostinfo;
          }
       }
    }

    *inputDevice = -1;
    *outputDevice = -1;

    printf("No Audio I/O device available, quit\n\r"); fflush(stdout);
    
    return NULL;
}

/*******************************************************************/
int main(void)
{
    const PaHostApiInfo *hostinfo;

    PaDeviceIndex inputDevice = -1;
    PaDeviceIndex outputDevice = -1;
    
    PaStreamParameters  inputParameters, outputParameters;
    PaStream *stream;
    PaError    err;
    
    int        i;
    int        totalFrames;
    int        numSamples;
    int        numBytes;
    SAMPLE     max, average, val;
    
    paDevice padev;
    
    paSampleData *Data;

    printf("patest_record.c\n"); fflush(stdout);
    
    padev.Samples = (paSampleData*)malloc(sizeof(paSampleData)*2);    
    memset(padev.Samples, 0, sizeof(paSampleData)*2);
    
    for(i=0; i<2; i++)
    {
      Data = &padev.Samples[i];
      Data->maxFrameIndex = totalFrames = NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */

      Data->frameIndex = 0;
      numSamples = totalFrames * NUM_CHANNELS;

      numBytes = numSamples * sizeof(SAMPLE);
      Data->pcm = (SAMPLE *) malloc( numBytes );
      for( i=0; i<numSamples; i++ )
         Data->pcm[i] = 0;
    }

    
    padev.RecData = 0;
    padev.PlayData = 1;
    padev.record = 1;
    padev.play = 0;
    padev.finish = 0;

    err = Pa_Initialize();
    if( err != paNoError ) goto error;

    padev_list_devices();
    
    hostinfo = padev_find_io(paALSA, &inputDevice, &outputDevice);

    inputParameters.device = inputDevice; /* default input device */
    inputParameters.channelCount = NUM_CHANNELS;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    
    outputParameters.device = outputDevice; /* default output device */
    outputParameters.channelCount = NUM_CHANNELS;          /* stereo output */
    outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              &outputParameters,
              SAMPLE_RATE,
              paFramesPerBufferUnspecified,    /* frames per buffer */
              paClipOff,                        /* we won't output out of range samples so don't bother clipping them */
              ioCallback,
              &padev );
              
    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;
    printf("Now recording!!\n"); fflush(stdout);

    while( Pa_IsStreamActive( stream ) )
    {
      Pa_Sleep(1000);
	   if(padev.record)
	   {
         printf("recindex = %d\n", padev.Samples[padev.RecData].frameIndex ); fflush(stdout);
  	   }
	   else
	   {
		   printf("playindex = %d\n", padev.Samples[padev.PlayData].frameIndex ); fflush(stdout);
	   }
    }

    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error;

    /* Measure maximum peak amplitude. */
    max = 0;
    average = 0;
    for( i=0; i<numSamples; i++ )
    {
        val = padev.Samples[padev.RecData].pcm[i];
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
    
    for(i=0; i<2; i++)
      free( padev.Samples[i].pcm);

    free(padev.Samples);

    Pa_Terminate();
    return 0;

error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return -1;
}
