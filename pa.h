#include <iostream>
#include <math.h>
#include "portaudio.h"
#include <cstdlib>
#include <sndfile.hh>

#define SAMPLE_RATE (48000)
#define FRAMES_PER_BUFFER (512)

using namespace std;

class Player {
	float *buffer;
	int frames;
	int phase;
public:
	Player(const char *filename) {
		SndfileHandle in(filename, SFM_READ);
		frames = in.frames() * in.channels();
		buffer = new float[frames];
		in.readf(buffer, frames);
		phase = 0;

		cout << "file load: " << filename
			<< " " << in.frames() / SAMPLE_RATE << " sec." << endl;
	}
	float out() {
		if (phase == frames) phase = 0;
		return buffer[phase++];
	}
	Player &set_phase(int i) {
		if (i > frames) i = frames;
		phase = i;
		return *this;
	}
	int get_frames() {
		return frames;
	}
};

class Filter {
	double cutoff;
	double resonance;
	double a1, a2, a3, b1, b2, c, in1, in2, out, out1, out2;
public:
	Filter(double cf, double res) {
		cutoff = cf;
		resonance = res;
		in1 = 0;
		in2 = 0;
		out1 = 0;
		out2 = 0;
	}
	double lowpass(double in) {
		c = 1.0 / tan(M_PI * cutoff / SAMPLE_RATE);
		a1 = 1.0 / ( 1.0 + resonance * c + c * c);
		a2 = 2 * a1;
		a3 = a1;
		b1 = 2.0 * ( 1.0 - c * c) * a1;
		b2 = ( 1.0 - resonance * c + c * c) * a1;

		out = a1*in + a2*in1 + a3*in2 - b1*out1 - b2*out2;

		in2 = in1;
		in1 = in;
		out2 = out1;
		out1 = out;

		return out;
	}
	Filter &set_cutoff(double co) {
		cutoff = co;
		return *this;
	}
};

class Sine {
	double _freq;
	double _phase;
public:
	Sine() {
		_phase = 0;
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

class Pulse {
	double _freq;
	double _phase;
public:
	Pulse() { _phase = 0; }
	Pulse(double f) {
		_freq = f;
		_phase = 0;
	}
	double val() {
		_phase += _freq / SAMPLE_RATE;
		return (double) (sin(2 * M_PI * _phase) > 0) ? 1.0 : -1.0;
		/*
		double out = sin(2 * M_PI * _phase);
		if (out > 0) return (double) 1.0;
		if (out < 0) return (double) -1.0;
		return 0;
		*/
	}
	Pulse &freq(double f) {
		_freq = f;
		return *this;
	}
	Pulse &phase(double p) {
		_phase = 0;
		return *this;
	}
	double freq() {
		return _freq;
	}
};

class Saw {
	double _freq;
	double _phase;
public:
	Saw() {}
	Saw(double f) {
		_freq = f;
		_phase = 0;
	}
	double val() {
		_phase += _freq / SAMPLE_RATE;
		_phase = (_phase > 1) ? -1 : _phase;
		return _phase;
	}
	Saw &freq(double f) {
		_freq = f;
		return *this;
	}
	Saw &phase(double p) {
		_phase = 0;
		return *this;
	}
	double freq() {
		return _freq;
	}
};

class Noise {
public:
	Noise() {};
	double val() {
		return (double) rand() / (RAND_MAX * 0.5) - 1.0;
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
		from = 0;
		to = 0;
		dur = 0;
		length = 0;
		phase = 0;
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
	bool done() {
		return (phase > length);
	}
};

class AR {
	Line a;
	Line r;
public:
	AR(double from, double to1, double dur1, double to2, double dur2) {
		a.set(from, to1, dur1);
		r.set(to1, to2, dur2);
	}
	double val() {
		if (a.done()) {
			return r.val();
		}
		return a.val();
	}
	void reset() {
		a.reset();
		r.reset();
	}
};

class ASR {
	Line a;
	Line s;
	Line r;
public:
	ASR() {}
	ASR(double from, double to1, double dur1, double to2, double dur2, double to3, double dur3) {
		a.set(from, to1, dur1);
		s.set(to1, to2, dur2);
		r.set(to2, to3, dur3);
	}
	double val() {
		if (a.done()) {
			if (s.done()) {
				return r.val();
			}
			return s.val();
		}
		return a.val();
	}
	void reset() {
		a.reset();
		s.reset();
		r.reset();
	}
	void set(double from, double to1, double dur1, double to2, double dur2, double to3, double dur3) {
		a.set(from, to1, dur1);
		s.set(to1, to2, dur2);
		r.set(to2, to3, dur3);
	}
	bool done() {
		return (r.done());
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
	float io(float in) {

		if (in_index >= SAMPLE_RATE) in_index = 0;

		out_index = in_index - (int) (SAMPLE_RATE * delay_time);
		if (out_index < 0) out_index = SAMPLE_RATE + out_index;

		buffer[in_index] = buffer[out_index] * feedback + in;

		in_index++;
		return buffer[out_index];
	}
};
