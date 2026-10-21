add_executable(syrtis-crosscheck harness/main.cpp)
target_compile_features(syrtis-crosscheck PRIVATE cxx_std_20)
set_target_properties(syrtis-crosscheck PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(syrtis-crosscheck PRIVATE syrtis-core)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    foreach(selector IN ITEMS usage-pace format provider-quota-pace-v3)
        add_test(NAME crosscheck-${selector}
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/harness/run_selector.py"
                $<TARGET_FILE:syrtis-crosscheck>
                "${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
                "${CMAKE_CURRENT_SOURCE_DIR}/golden"
                "${CMAKE_CURRENT_BINARY_DIR}/${selector}"
                "${selector}"
        )
        set_tests_properties(crosscheck-${selector} PROPERTIES
            ENVIRONMENT "TZ=Asia/Taipei"
            SKIP_RETURN_CODE 77
            TIMEOUT 30
        )
    endforeach()

    add_test(NAME crosscheck-selftest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/harness/selftest/check.py"
            $<TARGET_FILE:syrtis-crosscheck> "${CMAKE_CURRENT_BINARY_DIR}/selftest"
    )
    set_tests_properties(crosscheck-selftest PROPERTIES
        ENVIRONMENT "TZ=Asia/Taipei"
        TIMEOUT 30
    )
endif()
