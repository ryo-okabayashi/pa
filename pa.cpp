#include "pa.h"
#include <signal.h>
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

	srand(time(NULL));

	// record
	float rec_samples[FRAMES_PER_BUFFER*2];
	int rec_i;
	Record rec;
	if (REC) {
		rec.prepare();
	}

	Sine sine(440);

	unsigned long count = 0;
	while(1) {
		if (REC) rec_i = 0;
		for (i = 0; i < FRAMES_PER_BUFFER; i++) {

			samples[i][0] = sine.val() * 0.5;
			samples[i][1] = samples[i][0];

			if (REC) {
				rec_samples[rec_i++] = samples[i][0];
				rec_samples[rec_i++] = samples[i][1];
			}

			count++;
		}
		if (REC) rec.write(rec_samples);
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
