#include "pa.h"
#include <signal.h>
using namespace std;

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

	signal(SIGINT, signal_catch);
	PaStreamParameters outputParameters;
	PaStream *stream;
	PaError e;

	int i;
	float samples[FRAMES_PER_BUFFER][2];

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
			NULL,
			NULL );
	err(e);

	e = Pa_StartStream(stream);
	err(e);

	//srand(time(NULL));

	float out[FRAMES_PER_BUFFER];

	Saw saw(100);
	Filter filter(200, 0.5);
	AR ar(0, 1, 0.01, 0, 0.19);
	int seed = 0;

	Pulse pulse(100);
	AR ar2(0, 1, 0.01, 0, 0.02);

	Sine kick;
	Line kick_freq(500, 100, 0.05);
	Line kick_env(1, 0, 0.2);

	Noise snare;
	Line snare_env;

	Sine hh(9000);
	Line hh_env(1, 0, 0.01);

	unsigned long count = 0;
	while(1) {
		for (i = 0; i < FRAMES_PER_BUFFER; i++) {

			out[i] = 0;

			if (count%(int)(SAMPLE_RATE * 0.8) == (SAMPLE_RATE * 0.4)) {
				snare_env.set(1, 0, 0.05);
			}
			out[i] += snare.val() * snare_env.val() * 0.2;

			if (count%(int)(SAMPLE_RATE * 0.8) == 0) {
				kick_freq.reset();
				kick_env.reset();
			}
			out[i] += kick.freq(kick_freq.val()).val() * kick_env.val() * 0.2;

			if (count % (int)(SAMPLE_RATE * 12.8) == 0) {
				seed++;
				cout << seed << endl;
			}

			if (count % (int)(SAMPLE_RATE * 0.8) == 0) {
				srand(seed);
			}

			if (count%(int)(SAMPLE_RATE * 0.2) == 0) {
				saw.freq(rand() % 100 + 50);
				ar.reset();
			}
			if (count%(int)(SAMPLE_RATE * 0.1) == 0) {
				if (rand()%2==0) {
					pulse.freq(rand()%1000 + 500);
					ar2.reset();
				}
				if (rand()%2==0) {
					hh_env.reset();
				}
			}
			out[i] += (float) hh.val() * hh_env.val() * 0.2;
			out[i] += (float) filter.lowpass(saw.val()) * ar.val() * 0.3;
			out[i] += (float) pulse.val() * ar2.val() * 0.1;

			//if (out[i] > 1 || out[i] < -1) cerr << "clip!" << endl;
			if (out[i] > 1) out[i] = 1;
			if (out[i] < -1) out[i] = -1;
			samples[i][0] = out[i];
			samples[i][1] = out[i];

			count++;
		}
		e = Pa_WriteStream(stream, samples, FRAMES_PER_BUFFER);
		//err(e);
		if (e != paNoError) cerr << "write error" << endl;
	}

	e = Pa_StopStream(stream);
	err(e);

	e = Pa_CloseStream(stream);
	err(e);

	Pa_Terminate();

	return 0;
}
