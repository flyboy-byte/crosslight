add_executable(LibreHistoryTest
  LibreHistoryTest.cpp
  ${REPO_ROOT}/src/libre/LibreHistory.cpp
)

target_include_directories(LibreHistoryTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(LibreHistoryTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(LibreHistoryTest)
