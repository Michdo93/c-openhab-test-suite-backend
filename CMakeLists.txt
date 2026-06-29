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

# c-openhab-rest-client (submodule)
add_subdirectory(extern/c-openhab-rest-client)

# c-openhab-test-suite (submodule)
add_subdirectory(extern/c-openhab-test-suite)

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
