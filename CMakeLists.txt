# CMakeList.txt: PAK-server에 대한 CMake 프로젝트, 여기에 소스를 포함하고
# 프로젝트 특정 논리를 정의합니다.
#
cmake_minimum_required (VERSION 3.20)

# 지원되는 경우 MSVC 컴파일러에 대해 핫 다시 로드 사용하도록 설정합니다.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("PAK-nativePlugin" LANGUAGES CXX)

# CRT MDd로
if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg toolchain이 연결되어 있다는 전제에서, Config package를 찾는다.
find_package(RtAudio CONFIG REQUIRED)
find_package(Aubio CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 이 프로젝트의 실행 파일에 소스를 추가합니다.
add_executable (duplex "test/duplex.cpp")

target_link_libraries(duplex
  PRIVATE
    RtAudio::rtaudio
)

add_executable (duplexDSP "test/duplexDSP.cpp")

target_link_libraries(duplexDSP
  PRIVATE
    RtAudio::rtaudio
)

# DLL
add_library(duplexPlugin SHARED "test/duplexPlugin.cpp")

target_link_libraries(duplexPlugin
  PRIVATE
    RtAudio::rtaudio
)

# Unity Native Plugin DLL
add_library(PAKNativePlugin SHARED
  "src/audioQueue.cpp"
  "src/audioQueue.h"
  "src/main.cpp"
  "src/main.h"
  "src/plugin_state.h"
  "src/dsp.cpp"
  "src/dsp.h"
  "src/input.cpp"
  "src/input.h"
  "src/judge.cpp"
  "src/judge.h"
  "src/ChartParser.cpp"
)

target_link_libraries(PAKNativePlugin
  PRIVATE
    RtAudio::rtaudio
    Aubio::aubio
    nlohmann_json::nlohmann_json
)

# 설계 의도: DLL을 동적 로드하는 최소 실행 테스트를 별도 타겟으로 제공한다.
add_executable(duplexPluginTest "test/duplexPluginTest.cpp")
add_dependencies(duplexPluginTest duplexPlugin)
add_custom_command(TARGET duplexPluginTest POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:duplexPlugin>"
          "$<TARGET_FILE_DIR:duplexPluginTest>"
)

# 이 프로젝트의 실행 파일에 소스를 추가합니다.
add_executable (duplexPitch "test/duplexPitch.cpp")

target_link_libraries(duplexPitch
  PRIVATE
    RtAudio::rtaudio
    Aubio::aubio
)

# 이 프로젝트의 실행 파일에 소스를 추가합니다.
add_executable (duplexChord "test/duplexChord.cpp")

target_link_libraries(duplexChord
  PRIVATE
    RtAudio::rtaudio
    Aubio::aubio
)

# 이 프로젝트의 실행 파일에 소스를 추가합니다.
add_executable (duplexTiming "test/duplexTiming.cpp")

target_link_libraries(duplexTiming
  PRIVATE
    RtAudio::rtaudio
    Aubio::aubio
)

# 이 프로젝트의 실행 파일에 소스를 추가합니다.
add_executable (RhythmGame "test/RhythmGame.cpp" "test/ChartParser.cpp")

target_link_libraries(RhythmGame
  PRIVATE
    RtAudio::rtaudio
    Aubio::aubio
    nlohmann_json::nlohmann_json
)

# 이 프로젝트의 실행 파일에 소스를 추가합니다.
add_executable (RhythmGame_0.2v
  "test/RhythmGame_0.2v.cpp"
  "test/RhythmGame_0.2v.h"
  "test/ChartParser.cpp"
)

target_link_libraries(RhythmGame_0.2v
  PRIVATE
    RtAudio::rtaudio
    Aubio::aubio
    nlohmann_json::nlohmann_json
)

# DLL 기반 리듬 게임 클라이언트 테스트
add_executable(RhythmGameDLL
  "test/RhythmGameDLL.cpp"
  "test/ChartParser.cpp"
)

target_link_libraries(RhythmGameDLL
  PRIVATE
    nlohmann_json::nlohmann_json
)

# DLL 기반 기타 입력 테스트
add_executable(guitarInputTest
  "test/guitarInputTest.cpp"
)
add_dependencies(guitarInputTest PAKNativePlugin)
