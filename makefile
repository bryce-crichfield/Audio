build:
	clang++ -o main main.cpp AudioSystem.cpp libs/libportaudio.a libs/libsndfile.a -lasound -ljack -pthread