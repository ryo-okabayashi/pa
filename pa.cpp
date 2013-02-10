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
	outputParameters.device = 6;
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

	float out[FRAMES_PER_BUFFER][2];
	float amp;

	Player player("d.wav");
	ASR asr;
	Delay delay0(0.3, 0.3);
	Delay delay1(0.35, 0.3);

	unsigned long count = 0;
	while(1) {
		for (i = 0; i < FRAMES_PER_BUFFER; i++) {

			if (asr.done()) {
				player.set_phase(rand()%player.get_frames());
				asr.set(0, 1, 0.001, 1, rand()%20*0.01+0.01, 0, 0.001);
			}

			amp = asr.val();
			out[i][0] = player.out() * amp;
			out[i][1] = player.out() * amp;

			samples[i][0] = out[i][0] + delay0.io(out[i][0]*0.2);
			samples[i][1] = out[i][1] + delay1.io(out[i][1]*0.2);

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
