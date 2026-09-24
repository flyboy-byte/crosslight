add_executable(FlockMatcherTest
  FlockMatcherTest.cpp
  ${REPO_ROOT}/src/flock/FlockMatcher.cpp
  ${REPO_ROOT}/src/flock/FlockFrame.cpp
)

target_include_directories(FlockMatcherTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(FlockMatcherTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(FlockMatcherTest)
