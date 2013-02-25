#include "pa.h"
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <lo/lo.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <cstdlib>

#define OSC_PORT "7770"
#define OSC_SEND_IP "192.168.1.5"
#define OSC_SEND_PORT "9000"
#define REC 0
#define MUSIC_DIR "/home/ryo/dj"

unsigned long count = 0;
Record rec;

// osc send
lo_address address = lo_address_new(OSC_SEND_IP, OSC_SEND_PORT);

// DJ Deck
vector<string> list;
int max_list;
Play deck0;
Play deck1;
int deck0_play = 0;
int deck1_play = 0;
float deck0_amp = 0.0;
float deck1_amp = 0.0;
float deck0_vol = 0.0;
float deck1_vol = 0.0;

// pa callback
static int paCallback( const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData )
{
	float *out = (float*)outputBuffer;
	float left, right;
	float rec_samples[FRAMES_PER_BUFFER*2];
	float deck0_out[2], deck1_out[2];
	unsigned int i, rec_i = 0;
	(void) inputBuffer; /* Prevent unused variable warning. */
	for( i=0; i<framesPerBuffer; i++ )
	{
		left = 0;
		right = 0;
		if (deck0_play) {
			deck0_out[0] = deck0.out();
			deck0_out[1] = deck0.out();
			left += deck0_out[0]*deck0_amp * 0.5;
			right += deck0_out[1]*deck0_amp * 0.5;
			if (deck0_vol < deck0_out[0]) deck0_vol = deck0_out[0];
		}
		if (deck1_play) {
			deck1_out[0] = deck1.out();
			deck1_out[1] = deck1.out();
			left += deck1_out[0]*deck1_amp * 0.5;
			right += deck1_out[1]*deck1_amp * 0.5;
			if (deck1_vol < deck1_out[0]) deck1_vol = deck1_out[0];
		}

		*out++ = left;
		*out++ = right;
		if (REC) {
			rec_samples[rec_i++] = left;
			rec_samples[rec_i++] = right;
		}

		if (count%SAMPLE_RATE==0) {
			stringstream ss;
			int deck0_sec = (int) ((deck0.get_frames()/2 - deck0.get_frames() / 2 * deck0.get_position()) / SAMPLE_RATE);
			int deck1_sec = (int) ((deck1.get_frames()/2 - deck1.get_frames() / 2 * deck1.get_position()) / SAMPLE_RATE);
			ss << (int) ((1 - deck0.get_position()) * 100) << '%' << " - " << deck0_sec << "sec.";
			lo_send(address, "/remain1", "s", ss.str().c_str());
			ss.str("");
			ss << (int) ((1 - deck1.get_position()) * 100) << '%' << " - " << deck1_sec << "sec.";
			lo_send(address, "/remain2", "s", ss.str().c_str());
		}

		if (count%(int)(SAMPLE_RATE*0.1)==0) {
			lo_send(address, "/play_volume1", "f", deck0_vol);
			lo_send(address, "/play_volume2", "f", deck1_vol);
			deck0_vol *= 0.9;
			deck1_vol *= 0.9;
		}

		count++;
	}

	if (REC) rec.write(rec_samples);
	return 0;
}

void create_list(const char * dirname) {
	DIR *dp;
	struct stat st;
	struct dirent *dir;
	dp = opendir(dirname);
	vector<string> dirs;
	vector<string> flacs;

	while ((dir = readdir(dp)) != NULL) {
		if (dir->d_name[0] == '.') continue;
		string path = string(dirname) + "/" + string(dir->d_name);
		stat(path.c_str(), &st);
		if (path.length() > 5 && path.substr(path.length() - 5) == ".flac") {
			list.push_back(path);
		}
		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			create_list(path.c_str());
		}
	}
}

void set_random_flac(int i) {
	if (list.size() > 0) {
		int n = rand()%list.size();
		string filename = list[n];
		list.erase(list.begin() + n);
		stringstream ss;
		switch(i) {
			case 0:
				deck0_play = 0;
				deck0_amp = 0.0;
				deck0.delete_buffer();
				deck0 = Play(filename.c_str());
				deck0.set_loop(0);

				lo_send(address, "/play_toggle1", "i", 0);
				lo_send(address, "/volume1", "i", 0);
				lo_send(address, "/file_path1", "s", filename.c_str());
				ss << (1 - deck0.get_position()) * 100 << '%';
				lo_send(address, "/remain1", "s", ss.str().c_str());
				break;
			case 1:
				deck1_play = 0;
				deck1_amp = 0.0;
				deck1.delete_buffer();
				deck1 = Play(filename.c_str());
				deck1.set_loop(0);

				lo_send(address, "/play_toggle2", "i", 0);
				lo_send(address, "/volume2", "i", 0);
				lo_send(address, "/file_path2", "s", filename.c_str());
				ss << (1 - deck1.get_position()) * 100 << '%';
				lo_send(address, "/remain2", "s", ss.str().c_str());
				break;
		}
		ss.str("");
		ss << list.size() << "/" << max_list;
		lo_send(address, "/file_nums", "s", ss.str().c_str());
	}
}

// OSC
static int osc(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data)
{
	if (string(path) == "/volume1") {
		deck0_amp = argv[0]->f;
	} else if (string(path) == "/volume2") {
		deck1_amp = argv[0]->f;
	} else if (string(path) == "/play_toggle1") {
		deck0_play = argv[0]->i;
	} else if (string(path) == "/play_toggle2") {
		deck1_play = argv[0]->i;
	} else if (string(path) == "/next1") {
		set_random_flac(0);
	} else if (string(path) == "/next2") {
		set_random_flac(1);
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

/*
void* thread1(void *args)
{
}
*/

int main(void) {

	srand(time(NULL));

	create_list(MUSIC_DIR);
	max_list = list.size();

	PaStreamParameters outputParameters;
	PaStream *stream;
	PaError e;

	signal(SIGINT, signal_catch);

	/*
	pthread_t thread;
	pthread_attr_t thread_attr;
	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	if(pthread_create(&thread, &thread_attr, thread1, NULL) != 0) {
		perror("pthrad create error");
	}
	pthread_join(thread, NULL);
	*/

	if (REC) rec.prepare();

	// OSC
	lo_server_thread server;
	server = lo_server_thread_new(OSC_PORT, NULL);
	lo_server_thread_add_method(server, "/volume1", "f", osc, NULL);
	lo_server_thread_add_method(server, "/volume2", "f", osc, NULL);
	lo_server_thread_add_method(server, "/play_toggle1", "i", osc, NULL);
	lo_server_thread_add_method(server, "/play_toggle2", "i", osc, NULL);
	lo_server_thread_add_method(server, "/next1", "i", osc, NULL);
	lo_server_thread_add_method(server, "/next2", "i", osc, NULL);
	lo_server_thread_start(server);

	// reset
	stringstream ss;
	ss << list.size() << "/" << max_list;
	string out = ss.str();
	lo_send(address, "/file_nums", "s", out.c_str());

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

	// input
	int c;
	while ((c = getchar())) {
		if (c == 'q') {
			break;
		}
	}

	//pthread_exit(NULL);

	e = Pa_StopStream(stream);
	err(e);

	e = Pa_CloseStream(stream);
	err(e);

	Pa_Terminate();

	return 0;
}
