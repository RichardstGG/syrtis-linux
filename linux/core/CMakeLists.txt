cmake_minimum_required(VERSION 3.21)

project(SyrtisCore LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_library(syrtis-core STATIC AgentUsage.cpp UsagePace.cpp Localization.cpp)
target_sources(syrtis-core PRIVATE Format.cpp)
target_sources(syrtis-core PRIVATE ClientRegistry.cpp ModelGrouping.cpp ModelScope.cpp WindowResolution.cpp Grid.cpp)
target_sources(syrtis-core PRIVATE TBCore.cpp AgentsReport.cpp HourlyReport.cpp ModelReport.cpp FilterParityProbe.cpp)
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
target_sources(syrtis-crosscheck PRIVATE ../crosscheck/harness/core-leaf.cpp)
target_sources(syrtis-crosscheck PRIVATE ../crosscheck/harness/reports.cpp)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME crosscheck-reports
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../crosscheck/harness/run_selector.py"
            $<TARGET_FILE:syrtis-crosscheck>
            "${CMAKE_CURRENT_SOURCE_DIR}/../crosscheck/fixtures"
            "${CMAKE_CURRENT_SOURCE_DIR}/../crosscheck/golden"
            "${CMAKE_CURRENT_BINARY_DIR}/reports" reports)
    set_tests_properties(crosscheck-reports PROPERTIES
        ENVIRONMENT "TZ=Asia/Taipei" SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME reports-selftest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../crosscheck/harness/reports_check.py"
            $<TARGET_FILE:syrtis-crosscheck> "${CMAKE_CURRENT_BINARY_DIR}/reports-selftest")
    set_tests_properties(reports-selftest PROPERTIES TIMEOUT 30)
    add_test(NAME crosscheck-core-leaf
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../crosscheck/harness/run_selector.py"
            $<TARGET_FILE:syrtis-crosscheck>
            "${CMAKE_CURRENT_SOURCE_DIR}/../crosscheck/fixtures"
            "${CMAKE_CURRENT_SOURCE_DIR}/../crosscheck/golden"
            "${CMAKE_CURRENT_BINARY_DIR}/core-leaf" core-leaf)
    add_test(NAME core-leaf-selftest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../crosscheck/harness/core_leaf_check.py"
            $<TARGET_FILE:syrtis-crosscheck> "${CMAKE_CURRENT_BINARY_DIR}/core-leaf-selftest")
    set_tests_properties(core-leaf-selftest PROPERTIES TIMEOUT 30)
    set_tests_properties(crosscheck-core-leaf PROPERTIES
        ENVIRONMENT "TZ=Asia/Taipei" SKIP_RETURN_CODE 77 TIMEOUT 30)
endif()
