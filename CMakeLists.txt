cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# Conan
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY         ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin)
# Boost
find_package(Boost 1.78.0 REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/geom.h
    src/model_serialization.h
    src/model.h
    src/model.cpp
    src/tagged.h
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.cpp
    src/request_handler.h
    src/args.h
    src/loot_generator.cpp
    src/loot_generator.h
    src/collision_detector.cpp
    src/collision_detector.h
    src/game_application.cpp
    src/game_application.h
    src/application_listener.h
    src/state_serializer.cpp
    src/state_serializer.h
    src/serializing_listener.cpp
    src/serializing_listener.h
    src/record_repository.cpp
    src/record_repository.h
)

target_compile_definitions(game_server PRIVATE _GLIBCXX_USE_CXX11_ABI=0)

target_link_libraries(game_server PRIVATE
    Threads::Threads
    Boost::boost
    ${CONAN_LIBS}
)
