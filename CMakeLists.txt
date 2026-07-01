cmake_minimum_required(VERSION 3.16)
project(OpenHABTestSuiteBackendC VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(CURL REQUIRED)
find_package(Threads)

# nlohmann/json (for the C++ HTTP server wrapper)
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
  include(FetchContent)
  FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(json)
endif()

# cpp-httplib (header-only, same as the C REST client backend)
include(FetchContent)
FetchContent_Declare(httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.15.3)
FetchContent_MakeAvailable(httplib)

# c-openhab-rest-client (submodule) — build first so openhab_rest_client_static
# is available as a CMake target when the test-suite submodule is processed.
add_subdirectory(extern/c-openhab-rest-client)

# c-openhab-test-suite (submodule)
# Its CMakeLists.txt checks for extern/c-openhab-rest-client/ in its OWN
# source dir — that directory won't exist when used as a submodule here.
# We tell it to skip find_package by pointing it at the already-built target
# via a dummy extern directory symlink workaround is complex, so we simply
# add the include dir directly and skip the test-suite's own CMakeLists.txt,
# using its source files and headers directly instead.
set(C_TEST_SUITE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/c-openhab-test-suite")
set(C_REST_CLIENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/c-openhab-rest-client")

add_library(openhab_test_suite_static STATIC
  ${C_TEST_SUITE_DIR}/src/openhab_item_tester.c
  ${C_TEST_SUITE_DIR}/src/openhab_thing_tester.c
  ${C_TEST_SUITE_DIR}/src/openhab_rule_tester.c
  ${C_TEST_SUITE_DIR}/src/openhab_channel_tester.c
  ${C_TEST_SUITE_DIR}/src/openhab_persistence_tester.c
  ${C_TEST_SUITE_DIR}/src/openhab_sitemap_tester.c
)
target_compile_definitions(openhab_test_suite_static PUBLIC OPENHAB_STATIC)
target_include_directories(openhab_test_suite_static PUBLIC
  ${C_TEST_SUITE_DIR}/include
  ${C_REST_CLIENT_DIR}/include)
target_link_libraries(openhab_test_suite_static PUBLIC
  openhab_rest_client_static
  ${CURL_LIBRARIES}
  $<$<BOOL:${Threads_FOUND}>:Threads::Threads>)

# ── Backend executable ────────────────────────────────────────────────────────
add_executable(openhab_test_suite_backend src/main.cpp)

target_link_libraries(openhab_test_suite_backend PRIVATE
  openhab_test_suite_static
  openhab_rest_client_static
  httplib::httplib
  ${CURL_LIBRARIES}
  nlohmann_json::nlohmann_json
  $<$<BOOL:${Threads_FOUND}>:Threads::Threads>)

if(UNIX)
  target_link_libraries(openhab_test_suite_backend PRIVATE pthread)
endif()