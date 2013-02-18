#include <iostream>
#include <math.h>
#include "portaudio.h"
#include <cstdlib>
#include <sndfile.hh>

#define SAMPLE_RATE (48000)
#define FRAMES_PER_BUFFER (512)

using namespace std;

class Play {
	float *buffer;
	int loop;
	int frames, m;
	double phase, speed, delta;
public:
	Play() {}
	Play(const char *filename) {
		cout << filename << endl;
		SndfileHandle infile(filename, SFM_READ);
		frames = infile.frames() * infile.channels();
		buffer = new float[frames];
		infile.readf(buffer, frames);
		phase = 0;
		speed = 1.0;
		loop = 1;

		cout << infile.frames() / SAMPLE_RATE << " sec." << endl;
	}
	void delete_buffer() {
		delete buffer;
	}
	float out() {
		if (!frames) return 0;
		if (phase >= frames) {
			if (loop) {
				phase = 0;
			} else {
				return 0;
			}
		}
		else phase = phase + speed;
		if (phase < 0) phase = frames - 1;

		// return buffer[(int)phase];

		// 線形補間
		m = (int) phase;
		delta = phase - (double) m;
		return delta * buffer[m+1] + (1.0 - delta) * buffer[m];

	}
	Play &set_phase(int i) {
		if (i > frames) i = frames;
		phase = i;
		return *this;
	}
	Play &set_speed(float f) {
		speed = f;
		return *this;
	}
	int get_frames() {
		return frames;
	}
	float get_position() {
		return phase / frames;
	}
	void set_loop(int i) {
		loop = i;
	}
};

class Record {
	SndfileHandle outfile;
public:
	Record() {}
	void prepare() {
		const char *filename = "out.wav";
		outfile = SndfileHandle(filename, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_FLOAT, 2, SAMPLE_RATE);
	}
	void write(float *f) {
		outfile.write(f, FRAMES_PER_BUFFER*2);
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
	Noise() {}
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
	void set(double t, double d) {
		from = val();
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
	int in_index, m;
	double out_index, delta;
	float buffer[SAMPLE_RATE];
	float delay_time, feedback, output;
public:
	Delay(float dt, float fb) {
		delay_time = dt;
		feedback = fb;
		for (int i = 0; i < SAMPLE_RATE; i++) {
			buffer[i] = 0.0;
		}
		in_index = 0;
	}
	float io(float in) {

		if (in_index >= SAMPLE_RATE) in_index = 0;

		out_index = in_index - ((double)SAMPLE_RATE * delay_time);
		if (out_index < 0) out_index = (double)(SAMPLE_RATE-1) + out_index;

		// 線形補間
		m = (int) out_index;
		delta = out_index - (double) m;
		output = (float) (delta * buffer[m+1] + (1.0 - delta) * buffer[m]);

		buffer[in_index] = output * feedback + in;

		in_index++;
		return output;
	}
	Delay &set_delay_time(float f) {
		delay_time = f;
		return *this;
	}
};
