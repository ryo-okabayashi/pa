#include <iostream>
#include <math.h>
#include "portaudio.h"
#include <cstdlib>

#define SAMPLE_RATE (48000)
#define FRAMES_PER_BUFFER (512)

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