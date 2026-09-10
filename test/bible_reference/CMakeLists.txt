add_executable(BibleReferenceTest
  BibleReferenceTest.cpp
  ${REPO_ROOT}/src/bible/BibleReference.cpp
)

target_include_directories(BibleReferenceTest PRIVATE
  ${REPO_ROOT}/src/bible
)

target_link_libraries(BibleReferenceTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BibleReferenceTest)
