cmake_minimum_required(VERSION 3.21)

project(Syrtis LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(syrtis src/main.cpp)
target_compile_features(syrtis PRIVATE cxx_std_20)
set_target_properties(syrtis PROPERTIES CXX_EXTENSIONS OFF)

qt_add_qml_module(syrtis
    URI Syrtis
    QML_FILES qml/Main.qml src/tray/Tray.qml
    RESOURCES src/tray/tray-icon.svg
)
target_link_libraries(syrtis PRIVATE Qt6::Quick)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Test)
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

    qt_add_executable(syrtis-tray-test tests/tray/TrayTest.cpp)
    target_compile_features(syrtis-tray-test PRIVATE cxx_std_20)
    target_link_libraries(syrtis-tray-test PRIVATE Qt6::Quick Qt6::Test)
    qt_add_resources(syrtis-tray-test tray-fixtures
        PREFIX "/"
        FILES qml/Main.qml src/tray/Tray.qml src/tray/tray-icon.svg
    )
    add_test(NAME syrtis-tray COMMAND syrtis-tray-test)
    set_tests_properties(syrtis-tray PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 10
    )

    add_test(NAME syrtis-startup COMMAND syrtis --quit-after-start)
    set_tests_properties(syrtis-startup PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 10
    )
endif()
