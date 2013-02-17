#include "pa.h"
#include "fltk.h"
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
using namespace std;

#define REC 1

void err(PaError pa_e) {
	if (pa_e != paNoError) {
		Pa_Terminate();
		cerr << "Error number: " << pa_e << endl;
		cerr << "Error message: " << Pa_GetErrorText(pa_e) << endl;
		exit(0);
	}
}

void signal_catch(int sig) {
	Pa_Terminate();
	cout << "signal:" << sig << endl;
	cout << "terminated." << endl;
	exit(0);
} 

Record rec;
unsigned long count = 0;

Sine sine(440);
AR ar(0, 1, 0.001, 0, 0.5);
Line line(0.1, 0.9, 30);

static int paCallback( const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData )
{
	float *out = (float*)outputBuffer;
	float left, right;
	float rec_samples[FRAMES_PER_BUFFER*2];
	unsigned int i, rec_i = 0;
	(void) inputBuffer; /* Prevent unused variable warning. */
	for( i=0; i<framesPerBuffer; i++ )
	{
		if (count%(int)(SAMPLE_RATE*0.2)==0) {
			sine.freq(rand()%1000+100);
			ar.reset();
		}
		left = sine.val() * ar.val() * 0.5;
		right = left;

		*out++ = left;
		*out++ = right;
		count++;
		if (REC) {
			rec_samples[rec_i++] = left;
			rec_samples[rec_i++] = right;
		}
	}
	if (REC) rec.write(rec_samples);
	return 0;
}

int main(void) {

	signal(SIGINT, signal_catch);

	// GUI
	pthread_t thread;
	pthread_attr_t thread_attr;
	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	if(pthread_create(&thread, &thread_attr, thread1, NULL) != 0) {
		perror("pthrad create error");
	}
	pthread_join(thread, NULL);

	if (REC) rec.prepare();

	PaStreamParameters outputParameters;
	PaStream *stream;
	PaError e;

	e = Pa_Initialize();
	err(e);

	// devices
	/*
	int numDevices = Pa_GetDeviceCount();
	const PaDeviceInfo *deviceInfo;
	for (int i = 0; i < numDevices; i++) {
		deviceInfo = Pa_GetDeviceInfo( i );
		const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
		cout << i << ": " << hostInfo->name << endl;
	}
	*/

	outputParameters.device = Pa_GetDefaultOutputDevice();
	if (outputParameters.device == paNoDevice) {
		cerr << "Error: No default output device." << endl;
		exit(0);
	}
	outputParameters.channelCount = 2;
	outputParameters.sampleFormat = paFloat32;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;

	e = Pa_OpenStream(
			&stream,
			NULL,
			&outputParameters,
			SAMPLE_RATE,
			FRAMES_PER_BUFFER,
			paClipOff,
			paCallback,
			NULL );
	err(e);

	e = Pa_StartStream(stream);
	err(e);

	srand(time(NULL));

	pthread_exit(NULL);

	e = Pa_StopStream(stream);
	err(e);

	e = Pa_CloseStream(stream);
	err(e);

	Pa_Terminate();

	return 0;
}
