#include <iostream>
#include <math.h>
#include "portaudio.h"
#include <cstdlib>

#define SAMPLE_RATE (48000)
#define FRAMES_PER_BUFFER (512)

using namespace std;

class Sine {
	double _freq;
	double _phase;
public:
	Sine() {
	}
	Sine(double f) {
		_freq = f;
		_phase = 0;
	}
	double val() {
		_phase += _freq / SAMPLE_RATE;
		return (double) sin(2 * M_PI * _phase);
		// _phase++;
		// return (double) sin(2 * M_PI * _phase * _freq / SAMPLE_RATE);
	}
	Sine &freq(double f) {
		_freq = f;
		return *this;
	}
	Sine &phase(double p) {
		_phase = 0;
		return *this;
	}
	double freq() {
		return _freq;
	}
};

class TSine {
	private:
		int table_size;
		int _phase;
	public:
		TSine(int size) {
			table_size = size;
		}
		double val() {
			_phase++;
			if (_phase >= table_size) _phase = 0;
			return (double) sin(2 * M_PI * _phase / table_size);
		}
		TSine &size(int s) {
			table_size = s;
			return *this;
		}
};

class Line {
	double from;
	double to;
	double dur;
	double length;
	int phase;
public:
	Line() {
	}
	Line(double f, double t, double d) {
		from = f;
		to = t;
		dur = d;
		length = dur * SAMPLE_RATE;
		phase = 0;
	}
	double val() {
		phase++;
		if (phase > length) return to;
		return (double) (from + (to - from) * (phase / length));
	}
	void reset() {
		phase = 0;
	}
	void set(double f, double t, double d) {
		from = f;
		to = t;
		dur = d;
		length = dur * SAMPLE_RATE;
		phase = 0;
	}
};

class Delay {
	int in_index;
	int out_index;
	float buffer[SAMPLE_RATE];
	float delay_time;
	float feedback;
public:
	Delay(float dt, float fb) {
		delay_time = dt;
		feedback = fb;
		for (int i = 0; i < SAMPLE_RATE; i++) {
			buffer[i] = 0;
		}
		in_index = 0;
	}
	float in(float in) {

		if (in_index >= SAMPLE_RATE) in_index = 0;

		out_index = in_index - (int) (SAMPLE_RATE * delay_time);
		if (out_index < 0) out_index = SAMPLE_RATE + out_index;

		buffer[in_index] = buffer[out_index] * feedback + in;

		in_index++;
		return buffer[out_index];
	}
};

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

	//outputParameters.device = Pa_GetDefaultOutputDevice();
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

	Sine sin(1000);
	Line lin(1, 0, 0.2);
	Delay delay(0.3, 0.8);
	float out[FRAMES_PER_BUFFER];
	float delay_out[FRAMES_PER_BUFFER];

	Pa_Sleep(10000);

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

			/*
			if (count%44100 == 0) {
				cout << f << endl;
			}
			*/

			if (out[i] > 1 || out[i] < -1) cerr << "clip!" << endl;
			samples[i][0] = out[i];
			samples[i][1] = out[i];

			count++;
		}
		e = Pa_WriteStream(stream, samples, FRAMES_PER_BUFFER);
		//err(e);
	}

	e = Pa_StopStream(stream);
	err(e);

	e = Pa_CloseStream(stream);
	err(e);

	Pa_Terminate();

	return 0;
}
