pa: pa.cpp
	g++ -O2 -Wall -lm -lrt -lasound -ljack -lpthread -o pa pa.cpp libportaudio.a
