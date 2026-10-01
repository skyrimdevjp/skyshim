cmake_minimum_required(VERSION 3.24)
project(skyshim LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Runtime flavour. CommonLib (MIT snapshot) selects the Address Library format at compile time:
#   OFF -> Skyrim SE 1.5.97 (version-*.bin)      ON -> AE-style (versionlib-*.bin)
option(SKYRIM_SUPPORT_AE "Build for AE-style runtimes (1.6+/1.7.x)" OFF)
set(REX_OPTION_INI OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/CommonLibSSE-NG-MIT CommonLibSSE EXCLUDE_FROM_ALL)

# Engine-independent smoke test (runs without Skyrim).
add_executable(registry_test tests/smoke/registry_test.cpp)

# Early bootstrap (no CommonLib dependency).
add_executable(skyshim_loader src/bootstrap/loader_main.cpp)

# Runtime DLL: bootstrap hook + engine access (CommonLib).
add_library(skyshim SHARED src/runtime.cpp src/engine.cpp src/papyrus/register.cpp src/translation.cpp src/events/events.cpp src/mainthread.cpp src/scaleform/inject.cpp src/papyrus/ui_api.cpp src/papyrus/equip_api.cpp)
target_link_libraries(skyshim PRIVATE CommonLibSSE::CommonLibSSE)
target_compile_options(skyshim PRIVATE /utf-8 /permissive- /Zc:preprocessor)
target_compile_definitions(skyshim PRIVATE $<$<BOOL:${SKYRIM_SUPPORT_AE}>:SKYRIM_SUPPORT_AE=1>)
