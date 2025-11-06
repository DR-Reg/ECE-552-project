cmake_minimum_required(VERSION 3.10)
project(ECE552 CXX)

add_subdirectory(lib)
add_executable(main src/main.cc)

set_target_properties(main PROPERTIES COMPILE_OPTIONS "") 
set_target_properties(main PROPERTIES CXX_STANDARD 20)
set_target_properties(main PROPERTIES CXX_STANDARD_REQUIRED ON)
# set_target_properties(main PROPERTIES CMAKE_CXX_EXTENSIONS OFF)

target_include_directories(main PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
# 	set(CMAKE_CXX_STANDARD 11)
# 	set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
# endif()
