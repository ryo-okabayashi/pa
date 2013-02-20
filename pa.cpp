#include "pa.h"
//#include "fltk.h"
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <lo/lo.h>
using namespace std;

#define OSC_PORT "7770"
#define REC 1

unsigned long count = 0;
Record rec;

// synth
Saw saw(200);
LPF lpf(200, 1.0);

// pa callback
static int paCallback( const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData )
{
	double left, right;
	float *out = (float*)outputBuffer;
	float rec_samples[FRAMES_PER_BUFFER*2];
	unsigned int i, rec_i = 0;
	(void) inputBuffer; /* Prevent unused variable warning. */
	for( i=0; i<framesPerBuffer; i++ )
	{
		left = (float) lpf.io(saw.val());
		//left = (float) saw.val() * 0.5;
		right = left;

		*out++ = (float) left;
		*out++ = (float) right;
		count++;
		if (REC) {
			rec_samples[rec_i++] = (float) left;
			rec_samples[rec_i++] = (float) right;
		}
	}
	if (REC) rec.write(rec_samples);
	return 0;
}

// OSC
static int osc(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data)
{
	cout << path << ": "
		<< argv[0]->f << endl;
	//sine.freq((float)argv[0]->i);
	return 0;
}

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

int main(void) {

	PaStreamParameters outputParameters;
	PaStream *stream;
	PaError e;

	signal(SIGINT, signal_catch);

	// GUI
	/*
	pthread_t thread;
	pthread_attr_t thread_attr;
	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	if(pthread_create(&thread, &thread_attr, thread1, NULL) != 0) {
		perror("pthrad create error");
	}
	pthread_join(thread, NULL);
	*/

	if (REC) rec.prepare();

	// OSC
	lo_server_thread server;
	server = lo_server_thread_new(OSC_PORT, NULL);
	lo_server_thread_add_method(server, "/1/fader1", "f", osc, NULL);
	lo_server_thread_add_method(server, "/1/fader2", "f", osc, NULL);
	lo_server_thread_start(server);

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

	int c;
	while ((c = getchar())) {
		if (c == 'q') {
			break;
		}
	}

	//pthread_exit(NULL);

	e = Pa_StopStream(stream);
	err(e);

	e = Pa_CloseStream(stream);
	err(e);

	Pa_Terminate();

	return 0;
}
