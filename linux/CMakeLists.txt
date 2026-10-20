cmake_minimum_required(VERSION 3.21)

project(Syrtis LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(syrtis src/main.cpp)
target_compile_features(syrtis PRIVATE cxx_std_20)
set_target_properties(syrtis PROPERTIES CXX_EXTENSIONS OFF)

qt_add_qml_module(syrtis
    URI Syrtis
    QML_FILES qml/Main.qml
)
target_link_libraries(syrtis PRIVATE Qt6::Quick)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME syrtis-startup COMMAND syrtis --quit-after-start)
    set_tests_properties(syrtis-startup PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 10
    )
endif()
