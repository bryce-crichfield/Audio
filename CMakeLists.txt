set(CXX_STANDARD_REQUIRED ON)
set(CXX_STANDARD 17)

file(GLOB_RECURSE EngineSrc src/*.cpp)
add_library(Engine ${EngineSrc})
target_link_libraries(Engine PUBLIC asound pthread jack sndfile portaudio)
target_link_libraries(Engine PUBLIC SDL2 SDL2_image)

add_executable(main main.cpp)
target_include_directories(main PUBLIC src)
target_link_libraries(main PUBLIC Engine)
