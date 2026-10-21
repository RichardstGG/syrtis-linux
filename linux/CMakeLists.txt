cmake_minimum_required(VERSION 3.21)

project(Syrtis LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
find_package(KF6DBusAddons 6.13 REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)

add_library(syrtis-singleinstance STATIC
    src/singleinstance/SingleInstance.cpp
    src/singleinstance/SingleInstance.h
)
target_include_directories(syrtis-singleinstance PUBLIC src/singleinstance)
target_link_libraries(syrtis-singleinstance PUBLIC KF6::DBusAddons)

qt_add_executable(syrtis src/main.cpp)
target_compile_features(syrtis PRIVATE cxx_std_20)
set_target_properties(syrtis PROPERTIES CXX_EXTENSIONS OFF)

qt_add_qml_module(syrtis
    URI Syrtis
    QML_FILES qml/Main.qml
)
target_link_libraries(syrtis PRIVATE Qt6::Quick syrtis-singleinstance)

include(CTest)
if(BUILD_TESTING)
    find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Test)
    qt_add_executable(syrtis-singleinstance-helper tests/singleinstance/InstanceHelper.cpp)
    target_link_libraries(syrtis-singleinstance-helper PRIVATE syrtis-singleinstance Qt6::Gui)
    qt_add_executable(syrtis-singleinstance-test tests/singleinstance/SingleInstanceTest.cpp)
    target_link_libraries(syrtis-singleinstance-test PRIVATE Qt6::Test)
    add_test(NAME syrtis-singleinstance
        COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" --
            $<TARGET_FILE:syrtis-singleinstance-test> $<TARGET_FILE:syrtis-singleinstance-helper>
    )
    set_tests_properties(syrtis-singleinstance PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
    )

    qt_add_executable(syrtis-bridge-test
        src/bridge/Bridge.cpp
        src/bridge/Bridge.h
        tests/bridge/BridgeTest.cpp
        tests/bridge/FakeFfi.cpp
        tests/bridge/FakeFfi.h
    )
    target_compile_features(syrtis-bridge-test PRIVATE cxx_std_20)
    set_target_properties(syrtis-bridge-test PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(syrtis-bridge-test PRIVATE
        src/bridge
        ../Sources/CTB/include
    )
    target_link_libraries(syrtis-bridge-test PRIVATE Qt6::Concurrent Qt6::Test)
    add_test(NAME syrtis-bridge COMMAND syrtis-bridge-test)
    set_tests_properties(syrtis-bridge PROPERTIES TIMEOUT 30)

    add_test(NAME syrtis-startup COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" --
        $<TARGET_FILE:syrtis> --quit-after-start)
    set_tests_properties(syrtis-startup PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 10
    )
endif()
