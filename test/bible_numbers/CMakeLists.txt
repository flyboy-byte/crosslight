add_executable(BibleNumbersTest
  BibleNumbersTest.cpp
)

target_include_directories(BibleNumbersTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(BibleNumbersTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BibleNumbersTest)
