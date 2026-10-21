qt_add_executable(syrtis-quota-cards-test
    ../../src/quota/QuotaService.cpp ../../src/quota/QuotaService.h
    ../../src/quota/QuotaSelectionPolicy.cpp ../../src/quota/QuotaSelectionPolicy.h
    ../../src/bridge/Bridge.cpp ../../src/bridge/Bridge.h
    ../bridge/FakeFfi.cpp ../bridge/FakeFfi.h QuotaCardsTest.cpp)
target_include_directories(syrtis-quota-cards-test PRIVATE
    ../../src/quota ../../src/bridge ../bridge ../../../Sources/CTB/include)
target_link_libraries(syrtis-quota-cards-test PRIVATE
    syrtis-core Qt6::Quick Qt6::Concurrent Qt6::Test)
target_compile_definitions(syrtis-quota-cards-test PRIVATE
    QUOTA_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../qml")
add_test(NAME syrtis-quota-cards COMMAND syrtis-quota-cards-test)
set_tests_properties(syrtis-quota-cards PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic"
    TIMEOUT 30)
