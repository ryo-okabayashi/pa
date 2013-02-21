pa: pa.cpp pa.h
	g++ -O2 -Wall -lm -lrt -lasound -ljack -lpthread -lsndfile -llo -lncurses -o pa pa.cpp libportaudio.a
cldj: cldj.cpp pa.h
	g++ -O2 -Wall -lm -lrt -lasound -ljack -lpthread -lsndfile -llo -o cldj cldj.cpp libportaudio.a
