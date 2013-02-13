pa: pa.cpp pa.h fltk.h
	g++ -O2 -Wall -lm -lrt -lasound -ljack -lpthread -lsndfile -lfltk2 -o pa pa.cpp libportaudio.a
