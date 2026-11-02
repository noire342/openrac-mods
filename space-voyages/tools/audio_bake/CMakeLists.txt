cmake_minimum_required(VERSION 3.24)
project(space_native_audio_bake LANGUAGES CXX)
set(OPENRAC_PORT "" CACHE PATH "Read-only OpenRAC port source directory")
if(NOT EXISTS "${OPENRAC_PORT}/audio/sound_player.cpp")
 message(FATAL_ERROR "Pass -DOPENRAC_PORT=<checkout>/port")
endif()
add_executable(space_native_audio_bake main.cpp
 "${OPENRAC_PORT}/assets/sound/adpcm.cpp"
 "${OPENRAC_PORT}/assets/sound/sound_bank.cpp"
 "${OPENRAC_PORT}/assets/sound/vag.cpp"
 "${OPENRAC_PORT}/audio/envelope.cpp"
 "${OPENRAC_PORT}/audio/pitch.cpp"
 "${OPENRAC_PORT}/audio/reverb.cpp"
 "${OPENRAC_PORT}/audio/spu.cpp"
 "${OPENRAC_PORT}/audio/sound_player.cpp")
target_compile_features(space_native_audio_bake PRIVATE cxx_std_20)
target_include_directories(space_native_audio_bake PRIVATE "${OPENRAC_PORT}")
if(MSVC)
 target_compile_options(space_native_audio_bake PRIVATE /W4 /EHsc)
endif()
