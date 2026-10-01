add_executable(HistoricalCalendarTest
  HistoricalCalendarTest.cpp
)

target_include_directories(HistoricalCalendarTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(HistoricalCalendarTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(HistoricalCalendarTest)
