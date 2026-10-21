cmake_minimum_required(VERSION 3.21)

project(Syrtis LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick Concurrent)
find_package(KF6DBusAddons 6.13 REQUIRED)
find_package(Threads REQUIRED)
find_program(CARGO_EXECUTABLE cargo REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)

# Let Cargo track Rust sources, manifests, build scripts and the engine pin on
# every build; a CMake-only file list can silently leave a stale archive linked.
get_filename_component(SYRTIS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(RUST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo")
set(RUST_STATICLIB "${RUST_TARGET_DIR}/release/libtb_core_ffi.a")
add_custom_target(syrtis-rust
    COMMAND "${CMAKE_COMMAND}" -E env "CARGO_TARGET_DIR=${RUST_TARGET_DIR}"
        "${CARGO_EXECUTABLE}" build -p tb_core_ffi --release --locked --offline
    WORKING_DIRECTORY "${SYRTIS_ROOT}"
    BYPRODUCTS "${RUST_STATICLIB}"
    USES_TERMINAL
    VERBATIM
)
add_library(tb_core_ffi STATIC IMPORTED)
set_target_properties(tb_core_ffi PROPERTIES
    IMPORTED_LOCATION "${RUST_STATICLIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${SYRTIS_ROOT}/Sources/CTB/include"
)
add_dependencies(tb_core_ffi syrtis-rust)
target_link_libraries(tb_core_ffi INTERFACE Threads::Threads ${CMAKE_DL_LIBS} m rt util)

add_library(syrtis-bridge STATIC src/bridge/Bridge.cpp src/bridge/Bridge.h)
target_compile_features(syrtis-bridge PRIVATE cxx_std_20)
set_target_properties(syrtis-bridge PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(syrtis-bridge PUBLIC src/bridge)
target_link_libraries(syrtis-bridge PUBLIC Qt6::Concurrent PRIVATE tb_core_ffi)

# Keep core independently buildable; its tests/crosschecks belong to the core
# build, not the app's ctest registry. EXCLUDE_FROM_ALL builds only the library
# consumed here, without building the standalone crosscheck executable.
function(add_syrtis_core)
    set(BUILD_TESTING OFF)
    add_subdirectory(core EXCLUDE_FROM_ALL)
endfunction()
add_syrtis_core()

set(SYRTIS_QUOTA_SOURCES
    src/quota/QuotaSelectionPolicy.cpp src/quota/QuotaSelectionPolicy.h
    src/quota/QuotaService.cpp src/quota/QuotaService.h
)
add_library(syrtis-quota STATIC ${SYRTIS_QUOTA_SOURCES})
target_include_directories(syrtis-quota PUBLIC src/quota)
target_link_libraries(syrtis-quota PUBLIC syrtis-core syrtis-bridge)

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
    QML_FILES qml/Main.qml qml/QuotaView.qml qml/AgentLimitsCard.qml
        qml/QuotaSummaryLine.qml src/tray/Tray.qml
    RESOURCES src/tray/tray-icon.svg
)
target_link_libraries(syrtis PRIVATE Qt6::Quick syrtis-singleinstance syrtis-bridge syrtis-quota)

include(CTest)
if(BUILD_TESTING)
    find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
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

    qt_add_executable(syrtis-quota-test
        ${SYRTIS_QUOTA_SOURCES}
        src/bridge/Bridge.cpp src/bridge/Bridge.h
        tests/quota/QuotaServiceTest.cpp
        tests/bridge/FakeFfi.cpp tests/bridge/FakeFfi.h
    )
    target_include_directories(syrtis-quota-test PRIVATE
        src/quota src/bridge tests/bridge ../Sources/CTB/include)
    target_link_libraries(syrtis-quota-test PRIVATE syrtis-core Qt6::Concurrent Qt6::Test)
    add_test(NAME syrtis-quota COMMAND syrtis-quota-test)
    set_tests_properties(syrtis-quota PROPERTIES TIMEOUT 30)
    add_subdirectory(tests/quota)

    qt_add_executable(syrtis-bridge-smoke tests/bridge/BridgeSmokeTest.cpp)
    target_link_libraries(syrtis-bridge-smoke PRIVATE syrtis-bridge Qt6::Test)
    # Observe the actual FFI entry thread without replacing the Rust function.
    target_link_options(syrtis-bridge-smoke PRIVATE "LINKER:--wrap=tb_probe")
    add_test(NAME syrtis-bridge-smoke
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/bridge/run-smoke.sh"
            $<TARGET_FILE:syrtis-bridge-smoke>
    )
    set_tests_properties(syrtis-bridge-smoke PROPERTIES TIMEOUT 30)
    add_test(NAME syrtis-probe
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/bridge/run-smoke.sh"
            "${DBUS_RUN_SESSION_EXECUTABLE}" -- $<TARGET_FILE:syrtis> --probe
    )
    set_tests_properties(syrtis-probe PROPERTIES TIMEOUT 30)

    qt_add_executable(syrtis-tray-test tests/tray/TrayTest.cpp)
    target_compile_features(syrtis-tray-test PRIVATE cxx_std_20)
    target_link_libraries(syrtis-tray-test PRIVATE Qt6::Quick Qt6::Test)
    qt_add_resources(syrtis-tray-test tray-fixtures
        PREFIX "/"
        FILES qml/Main.qml qml/QuotaView.qml qml/AgentLimitsCard.qml
            qml/QuotaSummaryLine.qml src/tray/Tray.qml src/tray/tray-icon.svg
    )
    add_test(NAME syrtis-tray COMMAND syrtis-tray-test)
    set_tests_properties(syrtis-tray PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 10
    )

    # Starting the quota view now polls providers; keep startup hermetic too.
    add_test(NAME syrtis-startup
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/bridge/run-smoke.sh"
            "${DBUS_RUN_SESSION_EXECUTABLE}" -- $<TARGET_FILE:syrtis> --quit-after-start)
    set_tests_properties(syrtis-startup PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 10
    )
endif()
