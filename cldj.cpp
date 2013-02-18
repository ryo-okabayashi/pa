#include "pa.h"
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <lo/lo.h>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <cstdlib>

#define OSC_PORT "7770"
#define OSC_SEND_IP "192.168.1.6"
#define OSC_SEND_PORT "9000"
#define REC 0
#define MUSIC_DIR "/home/ryo/Music"

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
	unsigned int i, rec_i = 0;
	(void) inputBuffer; /* Prevent unused variable warning. */
	for( i=0; i<framesPerBuffer; i++ )
	{
		left = 0;
		right = 0;
		if (deck0_play) {
			left += deck0.out()*deck0_vol * 0.5;
			right += deck0.out()*deck0_vol * 0.5;
		}
		if (deck1_play) {
			left += deck1.out()*deck1_vol * 0.5;
			right += deck1.out()*deck1_vol * 0.5;
		}

		*out++ = left;
		*out++ = right;
		count++;
		if (REC) {
			rec_samples[rec_i++] = left;
			rec_samples[rec_i++] = right;
		}
	}
	if (count%SAMPLE_RATE==0) {
		lo_send(address, "/1/fader3", "f", 1.0 - deck0.get_position());
		lo_send(address, "/1/fader4", "f", 1.0 - deck1.get_position());
		lo_send(address, "/1/fader5", "f", list.size()/(float)max_list);
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
		switch(i) {
			case 0:
				deck0_play = 0;
				deck0_vol = 0.0;
				lo_send(address, "/1/fader1", "f", 0.0);
				lo_send(address, "/1/toggle1", "i", 0);
				lo_send(address, "/1/toggle3", "i", 0);

				deck0.delete_buffer();
				deck0 = Play(filename.c_str());
				deck0.set_loop(0);
				lo_send(address, "/1/fader3", "f", 1.0);
				lo_send(address, "/1/toggle3", "i", 1);
				break;
			case 1:
				deck1_play = 0;
				deck1_vol = 0.0;
				lo_send(address, "/1/fader2", "f", 0.0);
				lo_send(address, "/1/toggle2", "i", 0);
				lo_send(address, "/1/toggle4", "i", 0);

				deck1.delete_buffer();
				deck1 = Play(filename.c_str());
				deck1.set_loop(0);
				lo_send(address, "/1/fader4", "f", 1.0);
				lo_send(address, "/1/toggle4", "i", 1);
				break;
		}
	}
}

// OSC
static int osc(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data)
{
	if (string(path) == "/1/fader1") {
		deck0_vol = argv[0]->f;
	} else if (string(path) == "/1/fader2") {
		deck1_vol = argv[0]->f;
	} else if (string(path) == "/1/toggle1") {
		deck0_play = argv[0]->i;
	} else if (string(path) == "/1/toggle2") {
		deck1_play = argv[0]->i;
	} else if (string(path) == "/1/toggle3") {
		if (argv[0]->i == 0) {
			set_random_flac(0);
		}
	} else if (string(path) == "/1/toggle4") {
		if (argv[0]->i == 0) {
			set_random_flac(1);
		}
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
	lo_server_thread_add_method(server, "/1/fader1", "f", osc, NULL);
	lo_server_thread_add_method(server, "/1/fader2", "f", osc, NULL);
	lo_server_thread_add_method(server, "/1/toggle1", "i", osc, NULL);
	lo_server_thread_add_method(server, "/1/toggle2", "i", osc, NULL);
	lo_server_thread_add_method(server, "/1/toggle3", "i", osc, NULL);
	lo_server_thread_add_method(server, "/1/toggle4", "i", osc, NULL);
	lo_server_thread_start(server);

	// reset
	lo_send(address, "/1/fader1", "f", 0.0);
	lo_send(address, "/1/fader2", "f", 0.0);
	lo_send(address, "/1/fader3", "f", 0.0);
	lo_send(address, "/1/fader4", "f", 0.0);
	lo_send(address, "/1/toggle1", "i", 0);
	lo_send(address, "/1/toggle2", "i", 0);
	lo_send(address, "/1/toggle3", "i", 1);
	lo_send(address, "/1/toggle4", "i", 1);

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
