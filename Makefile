pa: pa.cpp pa.h fft.h
	g++ -O2 -Wall -lm -lrt -lasound -ljack -lpthread -lsndfile -llo -lncurses -o pa pa.cpp libportaudio.a
cldj: cldj.cpp pa.h
	g++ -O2 -Wall -lm -lrt -lasound -ljack -lpthread -lsndfile -llo -o cldj cldj.cpp libportaudio.a
cldj2: cldj2.cpp pa.h
	g++ -O2 -Wall -lm -lrt -lasound -ljack -lpthread -lsndfile -llo -o cldj2 cldj2.cpp libportaudio.a
