add_executable(WifiFrameTest
  WifiFrameTest.cpp
  ${REPO_ROOT}/src/wifiaudit/WifiFrame.cpp
)

target_include_directories(WifiFrameTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(WifiFrameTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WifiFrameTest)
