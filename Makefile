pa: pa.cpp pa.h
	g++ -O2 -Wall -lm -lrt -lasound -ljack -lpthread -lsndfile -o pa pa.cpp libportaudio.a
