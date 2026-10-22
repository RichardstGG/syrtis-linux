target_sources(syrtis-crosscheck PRIVATE ../crosscheck/harness/QuotaTrendSelector.cpp)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME crosscheck-quota-trend
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../crosscheck/harness/run_selector.py"
            $<TARGET_FILE:syrtis-crosscheck>
            "${CMAKE_CURRENT_SOURCE_DIR}/../crosscheck/fixtures"
            "${CMAKE_CURRENT_SOURCE_DIR}/../crosscheck/golden"
            "${CMAKE_CURRENT_BINARY_DIR}/quota-trend" quota-trend)
    set_tests_properties(crosscheck-quota-trend PROPERTIES
        ENVIRONMENT "TZ=Asia/Taipei" SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME quota-trend-selftest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../crosscheck/harness/check_quota_trend.py"
            $<TARGET_FILE:syrtis-crosscheck> "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(quota-trend-selftest PROPERTIES ENVIRONMENT "TZ=Asia/Taipei" TIMEOUT 30)
endif()
