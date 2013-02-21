#include "pa.h"
#include "fft.h"
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <lo/lo.h>
#include <ncurses.h>
using namespace std;

#define OSC_PORT "7770"
#define REC 1

unsigned long count = 0;
Record rec;

// FFT
const int N = 64;
double x_real[N];
double x_imag[N];
float fft_samples[FRAMES_PER_BUFFER];

// synth
Sine sine(440);
Sine sine1(1000);
Sine sine2(5000);

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
		left = (sine.val() + sine1.val() + sine2.val()) * 0.3;
		right = left;

		fft_samples[i] = left;
		*out++ = (float) left;
		*out++ = (float) right;
		if (REC) {
			rec_samples[rec_i++] = (float) left;
			rec_samples[rec_i++] = (float) right;
		}
		count++;
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

void* draw(void *args)
{
	while(1) {

		clear();

		float vol = 0;

		for (int n = 0; n < N; n++)
		{
			x_real[n] = fft_samples[n];
			x_imag[n] = 0.0;
			if (fft_samples[n] > vol) vol = fft_samples[n];
		}

		FFT(x_real, x_imag, N);

		for (int k = 0; k < 10; k++)
		{
			//printw("%d %f+j%f\n", k, x_real[k], x_imag[k]);
			int num = abs((int)x_real[k]) + abs((int)x_imag[k]);
			for (int i = 0; i < num+1; i++) {
				if (i == num) attron(COLOR_PAIR(2));
				else attron(COLOR_PAIR(1));
				//addch(' '|A_REVERSE);
				addch('|');
				attroff(COLOR_PAIR(0));
			}
			printw("\n");
		}

		vol = (int)(vol*30);
		for (int i = 0; i < vol; i++) {
			if (i >= 25) attron(COLOR_PAIR(4));
			else if (i >= 20) attron(COLOR_PAIR(2));
			else attron(COLOR_PAIR(3));
			addch('|');
			attroff(COLOR_PAIR(3));
		}
		printw("\n");

		printw("playing %d sec.\n", count/SAMPLE_RATE);
		refresh();

		sleep(1);
	}
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

	initscr();
	start_color();
	init_pair(1, COLOR_CYAN, COLOR_BLACK);
	init_pair(2, COLOR_YELLOW, COLOR_BLACK);
	init_pair(3, COLOR_GREEN, COLOR_BLACK);
	init_pair(4, COLOR_RED, COLOR_BLACK);
	pthread_t thread;
	pthread_attr_t thread_attr;
	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread, &thread_attr, draw, NULL);
	pthread_join(thread, NULL);

	int c;
	while ((c = getch())) {
		if (c == 'q') {
			break;
		}
	}

	endwin();

	//pthread_exit(NULL);

	e = Pa_StopStream(stream);
	err(e);

	e = Pa_CloseStream(stream);
	err(e);

	Pa_Terminate();

	return 0;
}
