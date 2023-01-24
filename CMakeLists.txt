set(CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)
set(CXX_EXTENSIONS OFF)

find_library(PORTAUDIO_LIB portaudio)
find_library(SNDFILE_LIB sndfile)

add_library(juke Juke.cpp)
target_link_libraries(juke ${PORTAUDIO_LIB} ${SNDFILE_LIB})

add_executable(test test.cpp)
target_link_libraries(test juke asound pthread jack)
