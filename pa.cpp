#include "pa.h"
#include "fft.h"
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <lo/lo.h>
#include <ncurses.h>
using namespace std;

#define OSC_PORT "7770"
#define REC 0

unsigned long count = 0;
Record rec;

// FFT
const int N = 64;
double x_real[N];
double x_imag[N];
float fft_samples[FRAMES_PER_BUFFER];

// !!! synth !!!
const int num = 10;
Sine sines[num];
AR ars[num];

Sine kick;
Line kick_freq(380, 100, 0.02);
AR env_kick(0, 1, 0.001, 0, 0.099);

Saw bass(70);
int bass_i = 0;
int bass_freqs[] = {70,0,70,75,0,0,80,0};
AR env_bass(0, 1, 0.0001, 0, 0.2);
LPF fil_bass(400, 1.0);

Sine s0;
int s0_i = 0;
int s0_freq[8];
AR env_s0(0, 1, 0.001, 0, 0.01);

Sine s1(12000);
Line env_s1(1, 0, 0.01);

void init() {
	srand(time(NULL));
	for (int i = 0; i < 8; i++) {
		s0_freq[i] = rand()%5000+500;
	}
}

void play(double &left, double &right) {
	if (count%(int)(SAMPLE_RATE*0.8)==0) {
		kick_freq.reset();
		env_kick.reset();
	} else if (count%(int)(SAMPLE_RATE*0.1)==0 && rand()%16==0) {
		kick_freq.reset();
		env_kick.reset();
	}
	if (count%(int)(SAMPLE_RATE*0.1)==0) {
		if (bass_i > 7) bass_i = 0;
		if (bass_freqs[bass_i] > 0) {
			bass.freq(bass_freqs[bass_i]);
			env_bass.reset();
		}
		bass_i++;
	}
	left += kick.freq(kick_freq.val()).val() * env_kick.val() * 0.5;
	left+= fil_bass.io(bass.val()) * env_bass.val() * 0.5;

	if (count%(int)(SAMPLE_RATE*0.1)==0) {
		if (s0_i > 7) s0_i = 0;
		s0.freq(s0_freq[s0_i++]);
		env_s0.reset();

		if (rand()%2==0) {
			env_s1.reset();
		}
	}
	left += s0.val() * env_s0.val() * 0.2;
	left += s1.val() * env_s1.val() * 0.1;

	for (int n = 0; n < num; n++) {
		if (ars[n].done()) {
			sines[n].freq(rand()%1000+100);
			ars[n].set(0, 1, rand()%1000*0.01, 0, rand()%1000*0.01);
		}
	}
	for (int n = 0; n < num; n++) {
		left += sines[n].val() * ars[n].val() * 0.02;
	}
	right = left;
}
// ___ synth ___ 

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
		left = right = 0;
		play(left, right);

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
	int numDevices = Pa_GetDeviceCount();
	const PaDeviceInfo *deviceInfo;
	for (int i = 0; i < numDevices; i++) {
		deviceInfo = Pa_GetDeviceInfo( i );
		const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
		cout << i << ": " << hostInfo->name << endl;
	}

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

	init();

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
