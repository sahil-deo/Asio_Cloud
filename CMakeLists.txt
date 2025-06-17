cmake_minimum_required(VERSION 3.10)

project(Du_Sprechen)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server src/server.cpp)
add_executable(client src/client.cpp)

target_include_directories(server PRIVATE 
${CMAKE_SOURCE_DIR}/include
)
target_include_directories(client PRIVATE 
${CMAKE_SOURCE_DIR}/include
)

add_definitions(-D_WIN32_WINNT=0x0A00)  # Targeting Windows 10