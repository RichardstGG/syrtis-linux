cmake_minimum_required(VERSION 3.21)

project(SyrtisCore LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_library(syrtis-core STATIC AgentUsage.cpp UsagePace.cpp Localization.cpp)
target_compile_features(syrtis-core PUBLIC cxx_std_20)
set_target_properties(syrtis-core PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(syrtis-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(syrtis-core PUBLIC Qt6::Core)

include(CTest)
if(BUILD_TESTING)
    add_executable(usage-pace-test tests/UsagePaceTest.cpp)
    target_link_libraries(usage-pace-test PRIVATE syrtis-core)
    add_test(NAME usage-pace-unit COMMAND usage-pace-test)
endif()
add_subdirectory(../crosscheck crosscheck)
