build:
	clang++ -o main main.cpp Juke.cpp libs/libportaudio.a libs/libsndfile.a -lasound -ljack -pthread -std=c++17