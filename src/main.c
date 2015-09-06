/* main.c - chromatic guitar tuner
 *
 * Copyright (C) 2012 by Bjorn Roche
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  This software is provided "as is" without express or
 * implied warranty.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include "libfft.h"
#include <portaudio.h>

/* -- some basic parameters -- */
#define SAMPLE_RATE (8000)
#define FFT_SIZE (8192)
#define FFT_EXP_SIZE (13)
#define NUM_SECONDS (20)

/* -- functions declared and used here -- */
void buildHanWindow( float *window, int size );
void applyWindow( float *window, float *data, int size );
//a must be of length 2, and b must be of length 3
void computeSecondOrderLowPassParameters( float srate, float f, float *a, float *b );
//mem must be of length 4.
float processSecondOrderFilter( float x, float *mem, float *a, float *b );
void signalHandler( int signum ) ;
void waitForStart();
void handleErrors(PaStream * stream, PaError err, void* ftt);
void handleSignals();
void initTables(float * mem1, float * mem2, float * freqTable, char** noteNameTable, float * notePitchTable);
int initPortAudio(PaError * err, PaStreamParameters * inputParametersp, PaStream ** stream);
void outputPitch(char* nearestNoteName, int nearestNoteDelta, float centsSharp);
void listen(PaError * errp, PaStream * stream, float * data, float * mem1, float * mem2, float * a,
   float * b, float * window, float * datai, float * freqTable, float * notePitchTable, 
   char ** noteNameTable, void * fft);

static bool running = true;

static char * NOTES[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

/* -- main function -- */
int main( int argc, char **argv ) {
   PaStreamParameters inputParameters;
   float a[2], b[3], mem1[4], mem2[4];
   float data[FFT_SIZE];
   float datai[FFT_SIZE];
   float window[FFT_SIZE];
   float freqTable[FFT_SIZE];
   char * noteNameTable[FFT_SIZE];
   float notePitchTable[FFT_SIZE];
   void * fft = NULL;
   PaStream *stream = NULL;
   PaError err = 0;

   handleSignals();
   buildHanWindow( window, FFT_SIZE );
   fft = initfft( FFT_EXP_SIZE );
   computeSecondOrderLowPassParameters( SAMPLE_RATE, 330, a, b );
   initTables(mem1, mem2, freqTable, noteNameTable, notePitchTable);

   int result = initPortAudio(&err, &inputParameters, &stream);
   if(result) goto error; //If result is non-zero, something is wrong

   waitForStart();
   listen(&err, stream, data, mem1, mem2, a, b, window, datai, freqTable, notePitchTable,
      noteNameTable, fft);

   err = Pa_StopStream( stream );
   if( err != paNoError ) goto error;

   // cleanup
   destroyfft( fft );
   Pa_Terminate();

   return 0;
 error:
   handleErrors(stream, err, fft);
   return 1;
}

//Listens to the microphone input and outputs the nearest pitch
void listen(PaError * errp, PaStream * stream, float * data, float * mem1, float * mem2, float * a,
   float * b, float * window, float * datai, float * freqTable, float * notePitchTable, 
   char ** noteNameTable, void * fft){

   char prevNote[3];
   prevNote[0] = '\n';
   int numInputs = 0; //number of inputs recorded 

   while( running )
   {
      numInputs++;
      // read some data
      *errp = Pa_ReadStream( stream, data, FFT_SIZE );
      for( int j=0; j<FFT_SIZE; ++j ) {
         data[j] = processSecondOrderFilter( data[j], mem1, a, b );
         data[j] = processSecondOrderFilter( data[j], mem2, a, b );
      }
      applyWindow( window, data, FFT_SIZE );

      // do the fft
      for( int j=0; j<FFT_SIZE; ++j )
         datai[j] = 0;
      applyfft( fft, data, datai, false );
      // char * nearestNoteName; float nearestNotePitch; float centsSharp; int nearestNoteDelta;
      // findPeakAndNearest(&nearestNoteName, &nearestNotePitch, &centsSharp, &nearestNoteDelta, 
      //    data, datai, freqTable, notePitchTable, noteNameTable);

      float maxVal = -1;
      int maxIndex = -1;
      for( int j=0; j<FFT_SIZE/2; ++j ) {
         float v = data[j] * data[j] + datai[j] * datai[j] ;
         if( v > maxVal ) {
            maxVal = v;
            maxIndex = j;
         }
      }      
      float freq = freqTable[maxIndex];
      //find the nearest note:
      int nearestNoteDelta = 0;
      while( true ) {
         if( nearestNoteDelta < maxIndex && noteNameTable[maxIndex-nearestNoteDelta] != NULL ) {
            nearestNoteDelta = -nearestNoteDelta;
            break;
         } else if( nearestNoteDelta + maxIndex < FFT_SIZE && noteNameTable[maxIndex+nearestNoteDelta] != NULL ) {
            break;
         }
         ++(nearestNoteDelta);
      }
      char * nearestNoteName = noteNameTable[maxIndex+nearestNoteDelta];
      float nearestNotePitch = notePitchTable[maxIndex+nearestNoteDelta];
      float centsSharp = 1200 * log( freq / nearestNotePitch ) / log( 2.0 );


      //updateScore();
      outputPitch(nearestNoteName, nearestNoteDelta, centsSharp);
   }
}

//Output the pitch heard and the degree of "pitchiness"
void outputPitch(char* nearestNoteName, int nearestNoteDelta, float centsSharp){
      printf("\033[2J\033[1;1H"); //clear screen, go to top left
      fflush(stdout);
      printf( "Nearest Note: %s\n", nearestNoteName );
      if( nearestNoteDelta != 0 ) {
         if( centsSharp > 0 )
            printf( "%f cents sharp.\n", centsSharp );
         if( centsSharp < 0 )
            printf( "%f cents flat.\n", -centsSharp );
      } else {
         printf( "in tune!\n" );
      }
      printf( "\n" );
      int chars = 30;
      if( nearestNoteDelta == 0 || centsSharp >= 0 ) {
         for( int i=0; i<chars; ++i )
            printf( " " );
      } else {
         for( int i=0; i<chars+centsSharp; ++i )
            printf( " " );
         for( int i=chars+centsSharp<0?0:chars+centsSharp; i<chars; ++i )
            printf( "=" );
      }
      printf( " %2s ", nearestNoteName );
      if( nearestNoteDelta != 0 )
         for( int i=0; i<chars && i<centsSharp; ++i )
           printf( "=" );
      printf("\n");
}

//Initializes PortAudio, which is what is used to gather input/pitches from the microphone.
//Returns 0 if initialization is successful; otherwise, returns 1
int initPortAudio(PaError * err, PaStreamParameters * inputParametersp, PaStream ** stream){
   *err = Pa_Initialize();
   int error = 1;
   if( *err != paNoError ) return error;

   inputParametersp->device = Pa_GetDefaultInputDevice();
   inputParametersp->channelCount = 1;
   inputParametersp->sampleFormat = paFloat32;
   inputParametersp->suggestedLatency = Pa_GetDeviceInfo( inputParametersp->device )->defaultHighInputLatency ;
   inputParametersp->hostApiSpecificStreamInfo = NULL;

   printf( "Opening %s\n",
           Pa_GetDeviceInfo( inputParametersp->device )->name );

   *err = Pa_OpenStream( stream,
                        inputParametersp,
                        NULL, //no output
                        SAMPLE_RATE,
                        FFT_SIZE,
                        paClipOff,
                        NULL,
                        NULL );
   if( *err != paNoError ) return error;

   *err = Pa_StartStream( *stream );
   if( *err != paNoError ) return error;
   return 0;
}

//Initialize values in frequency and note tables
void initTables(float * mem1, float * mem2, float * freqTable, char** noteNameTable, float * notePitchTable){
   mem1[0] = 0; mem1[1] = 0; mem1[2] = 0; mem1[3] = 0;
   mem2[0] = 0; mem2[1] = 0; mem2[2] = 0; mem2[3] = 0;

   for( int i=0; i<FFT_SIZE; ++i ) {
      freqTable[i] = ( SAMPLE_RATE * i ) / (float) ( FFT_SIZE );
   }
   for( int i=0; i<FFT_SIZE; ++i ) {
      noteNameTable[i] = NULL;
      notePitchTable[i] = -1;
   }
   for( int i=0; i<127; ++i ) {
      float pitch = ( 440.0 / 32.0 ) * pow( 2, (i-9.0)/12.0 ) ;
      if( pitch > SAMPLE_RATE / 2.0 )
         break;
      //find the closest frequency using brute force.
      float min = 1000000000.0;
      int index = -1;
      for( int j=0; j<FFT_SIZE; ++j ) {
         if( fabsf( freqTable[j]-pitch ) < min ) {
             min = fabsf( freqTable[j]-pitch );
             index = j;
         }
      }
      noteNameTable[index] = NOTES[i%12];
      notePitchTable[index] = pitch;
      //printf( "%f %d %s\n", pitch, index, noteNameTable[index] );
   }
}

//Set up signal handlers for correct exiting of program
void handleSignals(){  
   struct sigaction action;
   // add signal listen so we know when to exit:
   action.sa_handler = signalHandler;
   sigemptyset (&action.sa_mask);
   action.sa_flags = 0;

   sigaction (SIGINT, &action, NULL);
   sigaction (SIGHUP, &action, NULL);
   sigaction (SIGTERM, &action, NULL);
}

//Prints respective error messages if something goes wrong
void handleErrors(PaStream * stream, PaError err, void * fft){
   if( stream ) {
      Pa_AbortStream( stream );
      Pa_CloseStream( stream );
   }
   destroyfft( fft );
   Pa_Terminate();
   fprintf( stderr, "An error occured while using the portaudio stream\n" );
   fprintf( stderr, "Error number: %d\n", err );
   fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
}

//Program will not proceed until r is entered
void waitForStart(){
   char start = '\n';
   printf("Enter 'r' to start recording.\n");
   while(start != 'r'){
      start = getchar();
   }
}

//Creates window signal in order to reduce reading of frequencies that are not actually present
void buildHanWindow( float *window, int size )
{
   for( int i=0; i<size; ++i )
      window[i] = .5 * ( 1 - cos( 2 * M_PI * i / (size-1.0) ) );
}

//Multiplies audio input by window signal to reduce reading of frequencies that are not actually present
void applyWindow( float *window, float *data, int size )
{
   for( int i=0; i<size; ++i )
      data[i] *= window[i] ;
}

//Computes low pass parameters in order to filter out misleading higher frequencies
void computeSecondOrderLowPassParameters( float srate, float f, float *a, float *b )
{
   float a0;
   float w0 = 2 * M_PI * f/srate;
   float cosw0 = cos(w0);
   float sinw0 = sin(w0);
   //float alpha = sinw0/2;
   float alpha = sinw0/2 * sqrt(2);

   a0   = 1 + alpha;
   a[0] = (-2*cosw0) / a0;
   a[1] = (1 - alpha) / a0;
   b[0] = ((1-cosw0)/2) / a0;
   b[1] = ( 1-cosw0) / a0;
   b[2] = b[0];
}

//Applies the low pass filter to the audio to filter out misleading higher frequencies
float processSecondOrderFilter( float x, float *mem, float *a, float *b )
{
    float ret = b[0] * x + b[1] * mem[0] + b[2] * mem[1]
                         - a[0] * mem[2] - a[1] * mem[3] ;

		mem[1] = mem[0];
		mem[0] = x;
		mem[3] = mem[2];
		mem[2] = ret;

		return ret;
}

//Stops the recording
void signalHandler( int signum ) { running = false; }
