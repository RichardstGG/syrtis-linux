cmake_minimum_required(VERSION 3.21)

project(SyrtisCore LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_library(syrtis-core STATIC Core.cpp)
target_compile_features(syrtis-core PUBLIC cxx_std_20)
set_target_properties(syrtis-core PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(syrtis-core PUBLIC Qt6::Core)

include(CTest)
add_subdirectory(../crosscheck crosscheck)
