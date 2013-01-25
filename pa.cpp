#include "pa.h"
using namespace std;

void err(PaError pa_e) {
	if (pa_e != paNoError) {
		Pa_Terminate();
		cerr << "Error number: " << pa_e << endl;
		cerr << "Error message: " << Pa_GetErrorText(pa_e) << endl;
		exit(0);
	}
}

int main(void) {
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

	Sine sin(1000);
	Line lin(1, 0, 0.2);
	Delay delay(0.3, 0.8);
	float out[FRAMES_PER_BUFFER];
	float delay_out[FRAMES_PER_BUFFER];

	unsigned long count = 0;
	while(1) {
		for (i = 0; i < FRAMES_PER_BUFFER; i++) {

			if (count%(int)(SAMPLE_RATE * 0.1) == 0) {
				if (rand() % 4 == 0) {
					lin.reset();
					sin.freq(rand() % 5000 + 100);
				}
			}
			out[i] = (float) sin.val() * lin.val() * 0.4;

			delay_out[i] = delay.in(out[i]*0.5);
			out[i] += delay_out[i];

			if (out[i] > 1 || out[i] < -1) cerr << "clip!" << endl;
			samples[i][0] = out[i];
			samples[i][1] = out[i];

			count++;
		}
		e = Pa_WriteStream(stream, samples, FRAMES_PER_BUFFER);
		//err(e);
		if (e != paNoError) cout << "write error" << endl;
	}

	e = Pa_StopStream(stream);
	err(e);

	e = Pa_CloseStream(stream);
	err(e);

	Pa_Terminate();

	return 0;
}
