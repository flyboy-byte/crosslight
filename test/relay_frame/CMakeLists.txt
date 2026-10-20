add_executable(RelayFrameTest
  RelayFrameTest.cpp
  ${REPO_ROOT}/src/libre/RelayFrame.cpp
)

target_include_directories(RelayFrameTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(RelayFrameTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(RelayFrameTest)
